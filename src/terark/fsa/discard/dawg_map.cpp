#include "dawg_map.hpp"

#include <terark/fsa/dawg.hpp>
#include <terark/util/profiling.hpp>
#include <boost/thread.hpp>
#include <boost/bind.hpp>

dawg_map_i::~dawg_map_i() {
}

template<class State>
class dawg_map_impl : public dawg_map_i {
public:
	dawg_map_impl(size_t value_size0, const char* tmpDataFile) {
		b_async_compact = false;
		htab0.set_load_factor(0.8);
		stage = Stage::normal;
		is_searching = false;
		value_size = value_size0;
	}

	void set_async_compact(bool b = true) {
		b_async_compact = b;
	}
	void is_async_compact() const { return b_async_compact; }

	size_t size() const { return data.size() + htab0.end_i() + htab1.end_i(); }

	/// @param on_existed(Value& existed)
	///     pointers to 'existed' shouldn't escape out of functor 'on_existed'
	/// @return not has_existed
    bool insert(fstring word, const void* val, const OnExisted& on_existed) {
		if (b_async_compact)
			return slow_insert(word, val, on_existed);
		else
			return null_lock_insert(word, val, on_existed);
	}

	void compact() {
		if (b_async_compact) {
			boost::mutex::scoped_lock lock1(mtx1);
			boost::mutex::scoped_lock lock2(mtx2);
			if (!htab0.empty())
				do_compact<null_lock>();
			if (!htab0.empty()) // compact htab1
				do_compact<null_lock>();
		}
		else {
			do_compact<null_lock>();
		}
	}

private:
	struct null_lock {
		template<class AnyMutex>
		null_lock(AnyMutex&) {}
	};

    bool null_lock_insert(fstring word, const void* val, const OnExisted& on_existed) {
		if (!data.empty()) {
			size_t idx = dict.index(word.p, word.n);
			if (dict.null_word != idx) {
				assert(idx < data.size());
				on_existed(data0 + idx * value_size);
				return false;
			}
		}
		std::pair<size_t, bool> ib = htab0.insert_i(word, ValueOut());
		if (ib.second) {
			size_t threshold = std::max<size_t>(data.size(), 16u);
			if (htab0.size() >= threshold)
				do_compact<null_lock>();
		} else {
			on_existed(dyn_data_ptr + ib.first * value_size);
			return false;
		}
		return true;
	}

	/// @param on_existed(Value& existed)
	///          referrence/pointer of existed shouldn't escape
	///          out of functor on_existed
	/// @return not has_existed
    bool slow_insert(fstring word, const void* val, const OnExisted& on_existed) {
	  for (;;)
		switch (stage) {
		default:
			assert(0);
			throw std::runtime_error("unexpected stage");
		case Stage::normal: {
			assert(data.size() == dict.num_words());
			if (!htab1.empty()) {
				for (size_t i = 0; i < htab1.end_i(); ++i) {
					auto ib = htab0.insert_i(htab1.key(i), ValueOut());
					assert(ib.second); TERARK_UNUSED_VAR(ib);
				}
				htab1.clear();
			}
			if (!didx.empty()) {
				size_t idx = dict.index(word.p, word.n);
				if (dict.null_word != idx) {
					assert(idx < didx.size());
					size_t data_idx = didx[idx];
					on_existed(&data0[data_idx*value_size]);
					return false;
				}
			}
			std::pair<size_t, bool> ib = htab0.insert_i(word, ValueOut());
			if (ib.second) {
				size_t threshold = std::max<size_t>(data0.size(), 16u);
				data0.resize_no_init(data0.size() + value_size);
				memcpy(data0.end()-value_size, val, value_size);
				if (htab0.size() >= threshold) {
					assert(Stage::normal == stage);
					//
					// reserve memory here for:
					//   1. when merging, data0 couldn't be reallocated
					//   2. if put all new value to data1, when compact
					//      completed, compacting thread need reallocate
					//      data0 and append data1 to data0, this need
					//      additional memory
					//   3. in this way, when compacting thread completed,
					//      data1 is likely to be empty or small...
					data0.reserve(data0.size()*3/2);

					if (htab0.size() < 1)
						do_compact<null_lock>();
					else {
						stage = Stage::sorting;
						compact_async();
					}
				}
			} else {
				size_t data_idx = didx.size() + ib.first;
				on_existed(&data0[data_idx*value_size]);
				return false;
			}
			return true; }
		case Stage::sorting: {
			size_t idx = dict.index(word.p, word.n);
			if (dict.null_word != idx) {
				assert(idx < didx.size());
				size_t data_idx = didx[idx];
				on_existed(&data0[data_idx*value_size]);
				return false;
			}
			// htab0 is not cleared so far
			assert(!htab0.empty());
			idx = htab0.find_i(word);
			if (htab0.end_i() != idx) {
				size_t data_idx = didx.size() + idx;
				on_existed(&data0[data_idx*value_size]);
				return false;
			}
			std::pair<size_t, bool> ib = htab1.insert_i(word, val);
			if (!ib.second) {
				size_t data_idx = didx.size() + htab0.end_i() + ib.first;
				size_t byte_pos = data_idx*value_size;
				if (byte_pos < data0.size())
					on_existed(&data0[byte_pos]);
				else {
					assert(data0.size() == data0.capacity());
					on_existed(&data1[byte_pos-data0.size()]);
				}
				is_searching = false;
				return false;
			}
			if (data0.size() < data0.capacity()) {
				data0.resize_no_init(data0.size() + value_size);
				memcpy(data0.end()-value_size, val, value_size);
			} else {
				data1.resize_no_init(data1.size() + value_size);
				memcpy(data1.end()-value_size, val, value_size);
			}
			is_searching = false;
			return true; }
		case Stage::end_merging:
			assert(!is_searching);
			boost::thread::yield();
			break;
		case Stage::stealing: {
			{	// wait for lock2
				boost::mutex::scoped_lock lock2(mtx2);
			}
			break; }
		case Stage::endding: {
			terark::profiling pf;
			long long t0 = pf.now();
			{	// wait for lock2
				boost::mutex::scoped_lock lock2(mtx2);
			}
			long long t1 = pf.now();
			fprintf(stderr, "wait-endding time=%f\n", pf.sf(t0,t1));
			break; }
		case Stage::merging: {
			size_t idx = dict.index(word.p, word.n);
			if (dict.null_word != idx) {
				assert(idx < didx.size());
				size_t data_idx = didx[idx];
				on_existed(&data0[data_idx*value_size]);
				return false;
			}
			// htab0 has been cleared, and its offset+strpool was stealed
			// use binary search on sorted+offset+strpool
			Compare comp(ht0_offset, ht0_strpool)
			idx = std::lower_bound(ht0_sorted, ht0_soted+ht0_size, word, comp) - ht0_sorted;
			if (idx != ht0_size) {
				if (fstring::equal()(word, comp.str(ht0_sorted[idx]))) {
					size_t data_idx = didx.size() + idx;
					on_existed(&data0[data_idx*value_size]);
					return false;
				}
			}
			std::pair<size_t, bool> ib = htab1.insert_i(word, val);
			if (!ib.second) {
				size_t data_idx = didx.size() + htab0.end_i() + ib.first;
				size_t byte_pos = data_idx*value_size;
				if (byte_pos < data0.size())
					on_existed(&data0[byte_pos]);
				else {
					assert(data0.size() == data0.capacity());
					on_existed(&data1[byte_pos-data0.size()]);
				}
				is_searching = false;
				return false;
			}
			if (data0.size() < data0.capacity()) {
				data0.resize_no_init(data0.size() + value_size);
				memcpy(data0.end()-value_size, val, value_size);
			} else {
				data1.resize_no_init(data1.size() + value_size);
				memcpy(data1.end()-value_size, val, value_size);
			}
			is_searching = false;
			return true; }
		} // switch
	  assert(0); // never goes here
	}

	void compact_async() {
	   	// temporary thread object need not be joined
		boost::thread(boost::bind(&dawg_map_impl::compact_async_proc, this));
	}
	void compact_async_proc() {
		boost::mutex::scoped_lock lock1(mtx1);
		if (htab0.empty())
			return;
		terark::profiling pf;
		long long t0 = pf.now();
		do {
			do_compact<boost::mutex::scoped_lock>();
		} while (htab0.end_i() >= data.size());
		long long t1 = pf.now();
		stage = Stage::normal;
		fprintf(stderr, "End compact_async_proc, time = %f's\n", pf.sf(t0,t1));
	}

// compiler could optimize memcpy for const size block and delete dead code
// FastSize is compile time constant
// also, idx * FastSize may be optimized with bit shifts
// dst and src are all char* type
#define fast_copy(dst,dst_idx,src,src_idx) \
	if (FastSize == 0) \
		memcpy(dst + dst_idx * vsize   , src + src_idx * vsize   , vsize); \
	else \
		memcpy(dst + dst_idx * FastSize, src + src_idx * FastSize, FastSize)

	struct Compare {
		const unsigned* offsets;
		const char    * strpool;

		bool operator()(fstring x, unsigned y) const {
			return fstring::less_align(x, str(y));
		}
		bool operator()(unsigned y, fstring x) const {
			return fstring::less_align(x, str(y));
		}
		fstring str(unsigned y) const {
			size_t  yoff0  = hash_strmap<>::real_offset(offsets[y+0]);
			size_t  yoff1  = hash_strmap<>::real_offset(offsets[y+1]);
			size_t  length = hash_strmap<>::extralen(strpool, yoff1);
			return fstring(strpool + yoff0, length);
		}
		Compare(const unsigned* offsets, const char* strpool)
		  : offsets(offsets), strpool(strpool) {}
	};

	template<class LockType>
	void do_compact() {
		assert(Stage::sorting == stage);
		assert(!htab0.empty());
		assert(NULL == ht0_offset);
		assert(NULL == ht0_sorted);
		assert(NULL == ht0_strpool);
		ht0_sorted = htab0.get_sorted_index_fast();
		htab0.get_offsets(&ht0_offset);
		while (is_searching)
			boost::thread::yield();
		{
			LockType lock2(mtx2);
			stage = Stage::stealing;
			ValueOut* ignore;
			htab0.risk_steal_key_val_and_clear(&ht0_strpool, &ignore);
			stage = Stage::merging;
		}
		valvec<Value> didx2;
		DAWG<State> dict2; {
		  Automata<State> dict3; { // need not DAWG, Automata use less memory
			typename MinADFA_OnflyBuilder<Automata<State> >::Ordered onfly(&dict3);
			didx2.resize_no_init(didx.size() + htab0.end_i());
			size_t i = 0, j = 0;
			if (!didx.empty()) {
				dict.for_each_word(
				[&](size_t nth, const std::string& word, size_t) {
					while (j < htab0.end_i()) {
						size_t  real_idx = hindex[j];
						fstring dyn_word = htab0.key(real_idx);
						if (!fstring::Less()(dyn_word, word))
							break;
						didx2[i] = didx[dict.num_words() + real_idx];
						onfly.add_word(dyn_word.p, dyn_word.n);
						i++; j++;
					}
					didx2[i] = didx[nth];
					onfly.add_word(word);
					i++;
				});
			}
			for (; j < htab0.end_i(); ++j, ++i) {
				size_t  real_idx = hindex[j];
				fstring	dyn_word = htab0.key(real_idx);
				didx2[i] = dict.num_words() + real_idx;
				onfly.add_word(dyn_word.p, dyn_word.n);
			}
			// onfly is destructed here to do final minimization for dict3
		  }
		  dict3.shrink_to_fit();
		  dict3.path_zip(dict2, "DFS");
		}
		dict2.compile();
		stage = Stage::end_merging;
		while (is_searching)
			boost::thread::yield();
		{
			LockType lock2(mtx2);
			stage = Stage::endding;
			assert(ht0_offset ); ::free(ht0_offset ); ht0_offset  = NULL;
			assert(ht0_sorted ); ::free(ht0_sorted ); ht0_sorted  = NULL;
			assert(ht0_strpool); ::free(ht0_strpool); ht0_strpool = NULL;
			dict.clear(); dict.swap(dict2);
			didx.clear(); didx.swap(didx2);
			assert(didx.size() == dict.num_words());
			assert(htab0.empty());
			htab0.swap(htab1);
			if (!data1.empty()) {
				assert(data0.size() == data0.capacity());
				data0 += data1;
				data1.clear();
			}
			stage = Stage::sorting;
			// now htab0 may need compact to didx again
		}
	}

	struct Stage {
		enum Enum {
			normal = 0,
			sorting,
			stealing,
			merging,
			end_merging,
			endding,
		};
	};
	boost::mutex  mtx1, mtx2;
	valvec<uint32_t> didx;  // map dict ordinal to data index
    DAWG<State>   dict;
	hash_strmap<> htab0, htab1; // htab1 is for temp compacting
	valvec<char>  data0; // data0.size == value_size * (htab0.size+htab1.size)
	valvec<char>  data1; // for htab1 to store data
	size_t        value_size;
	size_t        ht0_size; // save htab0.size() after htab0 was stealed
	char*         ht0_strpool;
	unsigned*     ht0_offset;
	unsigned*     ht0_sorted;
	volatile long stage;
	volatile long is_searching;
	bool b_async_compact;
};

dawg_map_i* dawg_map_i::create(const char* state_class, size_t value_size, const char* tmpDataFile) {
	if (0== strcmp(state_class, "State4B"))
		return new dawg_map_impl<State4B>(value_size, tmpDataFile);
	if (0== strcmp(state_class, "State5B"))
		return new dawg_map_impl<State5B>(value_size, tmpDataFile);
	if (0== strcmp(state_class, "State6B"))
		return new dawg_map_impl<State6B>(value_size, tmpDataFile);
	if (0== strcmp(state_class, "State7B"))
		return new dawg_map_impl<State7B>(value_size, tmpDataFile);
	if (0== strcmp(state_class, "State8B"))
		return new dawg_map_impl<State8B>(value_size, tmpDataFile);
	if (0== strcmp(state_class, "State32"))
		return new dawg_map_impl<State32>(value_size, tmpDataFile);
	throw std::invalid_argument("dawg_map_i::create: invalid state_class");
}





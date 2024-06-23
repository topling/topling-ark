#ifndef __terark_dawg_map_hpp__
#define __terark_dawg_map_hpp__

#include "dawg.hpp"
#include <terark/util/profiling.hpp>
#include <boost/thread.hpp>
#include <boost/bind.hpp>


template<class MappedType, class State = State6B>
class dawg_map {
public:
    typedef MappedType mapped_type;

	dawg_map() {
		b_async_compact = false;
		dyn.set_load_factor(0.8);
		stage = Stage::normal;
		is_searching = false;
	}

	void set_async_compact(bool b = true) {
		b_async_compact = b;
	}
	void is_async_compact() const { return b_async_compact; }

	size_t size() const { return data.size() + dyn.end_i() + tmp.end_i(); }

	/// @param on_found(MappedType& val)
	///   pointer to 'val' shouldn't escape out of functor 'on_found'
	/// @return is_found
	/*
	template<class OnFound>
	bool find(const fstring word, OnFound on_found) {
		if (b_async_compact)
			return do_find<boost::mutex::scoped_lock>(word, on_found);
		else
			return do_find<null_lock>(word, on_found);
	}
	*/
	/// @param on_existed(MappedType& existed)
	///     pointers to 'existed' shouldn't escape out of functor 'on_existed'
	/// @return not has_existed
	template<class OnExisted>
    bool insert(fstring word, const MappedType& val, OnExisted on_existed) {
		if (b_async_compact)
			return slow_insert(word, val, on_existed);
		else
			return null_lock_insert(word, val, on_existed);
	}

	void compact() {
		if (b_async_compact) {
			boost::mutex::scoped_lock lock1(mtx1);
			boost::mutex::scoped_lock lock2(mtx2);
			if (!dyn.empty())
				do_compact<null_lock>();
			if (!dyn.empty()) // compact tmp
				do_compact<null_lock>();
		}
		else {
			do_compact<null_lock>();
		}
	}

	valvec<MappedType>& bldata() {
		assert(dyn.empty() && tmp.empty());
		return data;
	}
	DAWG<State>& risk_dfa() {
		assert(dyn.empty() && tmp.empty());
		return dict;
	}

private:
	struct null_lock {
		template<class AnyMutex>
		null_lock(AnyMutex&) {}
	};

	template<class OnExisted>
    bool null_lock_insert(fstring word, const MappedType& val, OnExisted on_existed) {
		assert(data.size() == dict.num_words());
		if (!data.empty()) {
			size_t idx = dict.index(word.p, word.n);
			if (dict.null_word != idx) {
				assert(idx < data.size());
				on_existed(data[idx]);
				return false;
			}
		}
		std::pair<size_t, bool> ib = dyn.insert_i(word, val);
		if (ib.second) {
			size_t threshold = std::max<size_t>(data.size(), 16u);
			if (dyn.size() >= threshold)
				do_compact<null_lock>();
		} else {
			on_existed(dyn.val(ib.first));
			return false;
		}
		return true;
	}

	/// @param on_existed(MappedType& existed)
	///          referrence/pointer of existed shouldn't escape
	///          out of functor on_existed
	/// @return not has_existed
	template<class OnExisted>
    bool slow_insert(fstring word, const MappedType& val, OnExisted on_existed) {
	  for (;;)
		switch (stage) {
		default:
			assert(0);
			throw std::runtime_error("unexpected stage");
		case Stage::normal: {
			assert(data.size() == dict.num_words());
			if (!tmp.empty()) {
				for (size_t i = 0; i < tmp.end_i(); ++i) {
					auto ib = dyn.insert_i(tmp.key(i), tmp.val(i));
					assert(ib.second); TERARK_UNUSED_VAR(ib);
				}
				tmp.clear();
			}
			if (!data.empty()) {
				size_t idx = dict.index(word.p, word.n);
				if (dict.null_word != idx) {
					assert(idx < data.size());
					on_existed(data[idx]);
					return false;
				}
			}
			std::pair<size_t, bool> ib = dyn.insert_i(word, val);
			if (ib.second) {
				size_t threshold = std::max<size_t>(data.size(), 16u);
				if (dyn.size() >= threshold) {
					if (Stage::normal == stage) {
						if (dyn.size() < 100000)
							do_compact<null_lock>();
						else {
							stage = Stage::starting_thread;
							compact_async();
						}
					}
				}
			} else {
				on_existed(dyn.val(ib.first));
				return false;
			}
			return true; }
		case Stage::starting_thread:
			boost::thread::yield();
			break;
		case Stage::end_merging:
			assert(!is_searching);
			boost::thread::yield();
			break;
		case Stage::pziping: {
			terark::profiling pf;
			long long t0 = pf.now();
			{	// wait for lock2
				boost::mutex::scoped_lock lock2(mtx2);
			}
			long long t1 = pf.now();
			fprintf(stderr, "wait-pziping time=%f\n", pf.sf(t0,t1));
			break; }
		case Stage::sorting: {
			is_searching = true;
			// dict+data is still available
			if (!data.empty()) {
				size_t idx = dict.index(word.p, word.n);
				if (dict.null_word != idx) {
					assert(idx < data.size());
					on_existed(data[idx]);
					is_searching = false;
					fprintf(stderr, "found when sorting: %s\n", word.p);
					return false;
				}
			}
			terark::profiling pf;
			long long t0 = pf.now();
			{	// wait for lock2
				boost::mutex::scoped_lock lock2(mtx2);
			}
			long long t1 = pf.now();
			fprintf(stderr, "wait-sorting time=%f\n", pf.sf(t0,t1));
			break; }
		case Stage::merging: {
			is_searching = true;
			if (!data.empty()) {
				size_t idx = dict.index(word.p, word.n);
				if (dict.null_word != idx) {
					assert(idx < data.size());
					on_existed(data[idx]);
					is_searching = false;
					return false;
				}
			}
			// sorted, but hash index has been destroyed
			size_t idx = dyn.lower_bound(word);
			if (dyn.end_i() != idx) {
				if (fstring::equal()(dyn.key(idx), word)) {
					on_existed(dyn.val(idx));
				//	fprintf(stderr, "1:lower_bound=%s word=%s\n", dyn.key(idx).p, word.p);
					is_searching = false;
					return false;
				} else {
				//	fprintf(stderr, "0:lower_bound=%s word=%s\n", dyn.key(idx).p, word.p);
				}
			} else {
				// fprintf(stderr, "^:lower_bound=%zd word=%s\n", idx, word.p);
			}
			std::pair<size_t, bool> ib = tmp.insert_i(word, val);
			if (!ib.second) {
				on_existed(tmp.val(ib.first));
				is_searching = false;
				return false;
			}
			is_searching = false;
			return true; }
		} // switch
	  assert(0); // never goes here
	}

	void compact_async() {
	   	// temporary thread object need not be joined
		boost::thread(boost::bind(&dawg_map::compact_async_proc, this));
	}
	void compact_async_proc() {
		boost::mutex::scoped_lock lock1(mtx1);
		if (dyn.empty())
			return;
		terark::profiling pf;
		long long t0 = pf.now();
		do {
			do_compact<boost::mutex::scoped_lock>();
		} while (dyn.end_i() >= data.size());
		long long t1 = pf.now();
		stage = Stage::normal;
		fprintf(stderr, "End compact_async_proc, time = %f's\n", pf.sf(t0,t1));
	}
	template<class LockType> void do_compact() {
		assert(!dyn.empty());
		{
			LockType lock2(mtx2);
			stage = Stage::sorting;
			dyn.shrink_to_fit();
			// utilize memory:
			//   hash search is not needed when non-async
			//   when async, hash table search degrade to binary search
			dyn.risk_disable_hash(); // free memory for hash index
			dyn.sort_fast();
			stage = Stage::merging;
		}
		size_t dyn_idx = 0;
		size_t idx = 0;
		valvec<MappedType> data2;
		DAWG<State>        dict2; {
#if 1
		typename MinADFA_OnflyBuilder<DAWG<State> >::Ordered onfly(&dict2);
#else
		typename MinADFA_OnflyBuilder<DAWG<State> >			 onfly(&dict2);
#endif
	   	data2.resize_no_init(data.size() + dyn.end_i());
		if (!data.empty()) {
			dict.for_each_word(
			[&](size_t nth, const std::string& word, size_t) {
				while (dyn_idx < dyn.end_i()) {
				//	assert(b_is_sorted);
					fstring     dyn_word = dyn.key(dyn_idx);
					MappedType& dyn_data = dyn.val(dyn_idx);
					if (!fstring::Less()(dyn_word, word))
						break;
					new(&data2[idx])MappedType(dyn_data); // copy_cons
					onfly.add_word(dyn_word.p, dyn_word.n);
					idx++; dyn_idx++;
				}
				new(&data2[idx++])MappedType(data[nth]); // copy_cons
				onfly.add_word(word);
			});
		}
		for (; dyn_idx < dyn.end_i(); ++dyn_idx, ++idx) {
			fstring		dyn_word = dyn.key(dyn_idx);
			MappedType& dyn_data = dyn.val(dyn_idx);
			new(&data2[idx])MappedType(dyn_data); // copy_cons
			onfly.add_word(dyn_word.p, dyn_word.n);
		}}
		stage = Stage::end_merging;
		while (is_searching)
			boost::thread::yield();
		{
			LockType lock2(mtx2);
			stage = Stage::pziping;
			dyn.clear();
			DAWG<State>().swap(dict); // clear dict
			data.clear();
			data.swap(data2);
			dict.swap(dict2);
			dict.path_zip("DFS"); // not too slow, atolerable
			dict.shrink_to_fit();
			dict.compile(); // not too slow, atolerable
			assert(data.size() == dict.num_words());
			dyn.swap(tmp);
			stage = Stage::starting_thread;
		}
	}

    DAWG<State>        dict;
    valvec<MappedType> data;
	hash_strmap<MappedType> dyn, tmp; // tmp is for temp compacting
	boost::mutex   mtx1, mtx2;
	struct Stage {
		enum Enum {
			normal = 0,
			starting_thread,
			sorting,
			merging,
			end_merging,
			pziping,
		};
	};
	volatile long stage;
	volatile long is_searching;
	bool b_async_compact;
};



#endif // __terark_dawg_map_hpp__


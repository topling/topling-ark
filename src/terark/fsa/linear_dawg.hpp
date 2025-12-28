#pragma once

#include <terark/num_to_str.hpp>
#include <terark/valvec.hpp>
#include <boost/mpl/if.hpp>
#include "dfa_algo.hpp"
#include "dfa_mmap_header.hpp"
#include "graph_walker.hpp"

namespace terark {

template<int StateBytes, int Sigma = 256>
class LinearDAWG : public AcyclicPathDFA, public SuffixCountableDAWG {
	BOOST_STATIC_ASSERT(Sigma <= 256);
	enum { char_bits = StaticUintBits<Sigma-1>::value };
public:
	typedef typename boost::mpl::if_c
			< StateBytes <= 5
			, uint32_t
			, uint64_t
			>
			::type  state_id_t
			;
	typedef typename boost::mpl::if_c
			<(StateBytes % 4 == 0)
			, state_id_t
			, unsigned char
			>
			::type  char_uint_t
			;
	typedef typename boost::mpl::if_c
			< char_bits + 2 <= 8
			, char_uint_t
			, state_id_t
			>
			::type  flag_uint_t
			;
	enum {  word_id_bits = StateBytes*8 - 2 - char_bits };
	enum { state_id_bits = StateBytes*8 - 0 - char_bits };
	using BaseDAWG::n_words;
public:
	enum { sigma = Sigma };
	#pragma pack(push,1)
	struct State {
		union u_ {
			struct state_ {
				state_id_t  n_cnt : word_id_bits; // size of right language
				flag_uint_t pzip_bit : 1;
				flag_uint_t term_bit : 1;
				char_uint_t n_trans  : char_bits; // range: [1, Sigma] inclusive
			} s;
			struct edge_ {
				state_id_t dest : StateBytes*8 - char_bits;
				char_uint_t ch  : char_bits;
			} e;
			struct zpath_ {
				unsigned char data[StateBytes];
			} z;
		} u;
		State() {
			memset(this, 0, sizeof(*this));
		}
	};
	#pragma pack(pop)
	BOOST_STATIC_ASSERT(sizeof(State) == StateBytes);

	typedef State state_t;
	typedef state_id_t transition_t;
	static const state_id_t max_state = state_id_t((uint64_t(1) << state_id_bits) - 2);
	static const state_id_t nil_state = state_id_t((uint64_t(1) << state_id_bits) - 1);
	static const size_t max_cnt_in_state = state_id_t((uint64_t(1) << word_id_bits) - 1);
	using BaseDAWG::null_word;

	LinearDAWG() {
		m_dyn_sigma = sigma;
		m_is_dag = true;
		m_gnode_states = 0;
		n_words = 0;
		is_compiled = false;
   	}

	void clear() {
		states.clear();
		m_zpath_states = 0;
	}
	void erase_all() {
		states.erase_all();
		m_zpath_states = 0;
		is_compiled = false;
	}
	void swap(LinearDAWG& y) {
		BaseDFA::risk_swap(y);
		states.swap(y.states);
		std::swap(m_gnode_states, y.m_gnode_states);
		std::swap(is_compiled , y.is_compiled);
	}

	bool has_freelist() const override final { return false; }

	size_t total_states() const { return states.size(); }
	size_t mem_size() const override final { return sizeof(State) * states.size(); }
	size_t num_used_states() const { return states.size(); }
	size_t num_free_states() const { return 0; }
	size_t v_gnode_states()  const override final { return m_gnode_states; }

	template<class Au>
	void build_from(const char* WalkerName, const Au& au, typename Au::state_id_t root = initial_state) {
		if (strcasecmp(WalkerName, "BFS") == 0) {
			build_from_aux<BFS_GraphWalker<typename Au::state_id_t> >(au, root);
			return;
		}
		if (strcasecmp(WalkerName, "DFS") == 0) {
			build_from_aux<DFS_GraphWalker<typename Au::state_id_t> >(au, root);
			return;
		}
		if (strcasecmp(WalkerName, "PFS") == 0) {
			build_from_aux<PFS_GraphWalker<typename Au::state_id_t> >(au, root);
			return;
		}
		throw std::invalid_argument("LinearDAWG::build_from: unknown WalkerName");
	}
	size_t v_num_children(size_t) const override { TERARK_DIE("should not call"); }
	bool v_has_children(size_t s) const override final {
		assert(s + 1 < states.size());
		return states[s].u.s.n_trans || nil_state != states[s+1].u.e.dest;
	}
	template<class OP>
	void for_each_dest(state_id_t curr, OP op) const {
		assert(curr + 1 < states.size());
		size_t n = states[curr].u.s.n_trans + 1;
		if (1 == n) {
			auto dest = states[curr+1].u.e.dest;
			if (nil_state != dest)
				op(dest);
			return;
		}
		size_t i = 0;
		do { ++i;
			assert(curr+i < states.size());
			state_id_t dest = states[curr+i].u.e.dest;
			assert(nil_state != dest);
			op(dest);
		} while (i < n);
	}
	template<class OP>
	void for_each_dest_rev(state_id_t curr, OP op) const {
		assert(curr + 1 < states.size());
		size_t n = states[curr].u.s.n_trans + 1;
		if (1 == n) {
			auto dest = states[curr+1].u.e.dest;
			if (nil_state != dest)
				op(dest);
			return;
		}
		size_t i = n+1;
		do { --i;
			assert(curr+i < states.size());
			state_id_t dest = states[curr+i].u.e.dest;
			assert(nil_state != dest);
			op(dest);
		} while (i > 1);
	}
	template<class OP>
	void for_each_move(state_id_t curr, OP op) const {
		assert(curr + 1 < states.size());
		size_t n = states[curr].u.s.n_trans + 1;
		if (1 == n) {
			auto e = states[curr+1].u.e;
			if (nil_state != e.dest)
				op(e.dest, e.ch);
			return;
		}
		size_t i = 0;
		do { ++i;
			assert(curr+i < states.size());
			state_id_t dest = states[curr+i].u.e.dest;
			assert(nil_state != dest);
			op(dest, states[curr+i].u.e.ch);
		} while (i < n);
	}
	state_id_t state_move(state_id_t curr, auchar_t ch) const {
		assert(curr + 1 < states.size());
		size_t n = states[curr].u.s.n_trans + 1;
		assert(curr + n < states.size());
	#if 1 // add linear search will increase code size
		if (n < 8) { // linear search for small array
			for (size_t i = 1; i < n+1; ++i) {
				if (states[curr+i].u.e.ch == ch)
					return states[curr+i].u.e.dest;
			}
			return nil_state;
		}
	#endif
		// binary search
		size_t lo = curr + 1, hi = curr + n+1;
		while (lo < hi) {
			size_t mid = (lo + hi) / 2;
			if (states[mid].u.e.ch < ch)
				lo = mid + 1;
			else
				hi = mid;
		}
		if (states[lo].u.e.ch == ch)
			return states[lo].u.e.dest;
		else
			return nil_state;
	}
	bool is_pzip(size_t s) const {
		assert(s < states.size());
		return states[s].u.s.pzip_bit;
	}
	bool is_term(size_t s) const {
		assert(s < states.size());
		return states[s].u.s.term_bit;
	}
	bool is_free(size_t s) const {
		assert(s < states.size());
		return false;
	}
	fstring get_zpath_data(size_t s, MatchContext*) const {
		assert(s+1 < states.size());
		assert(states[s].u.s.pzip_bit);
		size_t n_trans = states[s].u.s.n_trans + 1;
		const unsigned char* p = states.ref(s+1+n_trans).u.z.data;
		size_t zlen = *p;
#ifndef NDEBUG
		size_t zp_slots = slots_of_bytes(zlen+1);
		assert(s + zp_slots < states.size());
#endif
	//	printf("LinearDAWG::get_zpath_data(%zd) = %02X: %.*s\n", s, *p, *p, p+1);
		return fstring((const char*)p+1, zlen);
	}
    size_t index(MatchContext& ctx, fstring str) const override final {
	//	assert(n_words > 0);
        size_t curr = ctx.root;
        size_t idx = 0;
		size_t root_num = initial_state == ctx.root
						? n_words : inline_suffix_cnt(curr);
        for (size_t i = ctx.pos; ; ++i) {
			if (this->is_pzip(curr)) {
				fstring zs = this->get_zpath_data(curr, NULL);
				size_t j = ctx.zidx;
				assert(j <= zs.size());
				if (i + zs.size() - j > str.size())
					return null_word;
				do { // prefer do .. while for performance
					if (str.p[i++] != zs.p[j++])
						return null_word;
				} while (j < zs.size());
				ctx.zidx = 0;
			}
			if (str.size() == i)
				return this->is_term(curr) ? idx : null_word;
			if (this->is_term(curr)) {
				idx++;
				assert(idx < root_num);
			}
			size_t n_trans = states[curr].u.s.n_trans + 1;
			if (1 == n_trans && nil_state == states[curr+1].u.e.dest) {
			//	printf("null_word on a final state which has no any child\n");
				return null_word;
			}
			const byte_t ch = str.p[i];
			for (size_t j = 0; j < n_trans; ++j) {
				const auto e = states[curr+1+j].u.e;
			//	printf("i=%zd n_trans=%zd j=%zd e.ch=%02X e.dest.n_cnt=%zd\n"
			//		, i, n_trans, j, e.ch, states[e.dest].u.s.n_cnt);
				if (e.ch < ch) {
					idx += states[e.dest].u.s.n_cnt;
					// idx == root_num when str is larger than all words
					assert(idx <= root_num);
				} else if (e.ch == ch) {
					curr = e.dest;
					goto Found; // needn't to check idx==root_num
				} else
					return null_word;
			}
			if (idx == root_num) {
				// if str is larger than all words, it will go here!
				return null_word;
			}
		Found:;
		}
		assert(0);
        return size_t(-1); // depress compiler warning
    }

    DawgIndexIter dawg_lower_bound(MatchContext& ctx, fstring qry) const override final {
	//	assert(n_words > 0);
        size_t curr = ctx.root;
        size_t idx = 0;
		size_t root_num = initial_state == ctx.root
						? n_words : inline_suffix_cnt(curr);
        for (size_t i = ctx.pos; ; ++i) {
			if (this->is_pzip(curr)) {
				fstring zs = this->get_zpath_data(curr, NULL);
				size_t j = ctx.zidx;
				assert(j <= zs.size());
				assert(zs.size() > 0);
				size_t n = std::min(zs.size(), qry.size() - i);
				for (; j < n; ++i, ++j) {
					if (qry[i] != zs[j])
						return {qry[i] < zs[j] ? idx : idx + 1, i, false};
				}
				ctx.zidx = 0;
			}
			if (qry.size() == i)
				return {idx, i, this->is_term(curr)};
			if (this->is_term(curr)) {
				idx++;
				assert(idx < root_num);
			}
			size_t n_trans = states[curr].u.s.n_trans + 1;
			if (1 == n_trans && nil_state == states[curr+1].u.e.dest) {
			//	printf("null_word on a final state which has no any child\n");
				return {idx, i, false};
			}
			const byte_t ch = qry.p[i];
			for (size_t j = 0; j < n_trans; ++j) {
				const auto e = states[curr+1+j].u.e;
			//	printf("i=%zd n_trans=%zd j=%zd e.ch=%02X e.dest.n_cnt=%zd\n"
			//		, i, n_trans, j, e.ch, states[e.dest].u.s.n_cnt);
				if (e.ch < ch) {
					idx += states[e.dest].u.s.n_cnt;
					// idx == root_num when qry is larger than all words
					assert(idx <= root_num);
				} else if (e.ch == ch) {
					curr = e.dest;
					goto Found; // needn't to check idx==root_num
				} else
					return {idx, i, false};
			}
			if (idx == root_num) {
				// if qry is larger than all words, it will go here!
				return {idx, i, false};
			}
		Found:;
		}
		assert(0);
        return {size_t(-1), size_t(-1), false}; // depress compiler warning
    }

    template<class OnMatch, class TR>
    size_t
	tpl_match_dawg(MatchContext& ctx, size_t base_nth, fstring str, OnMatch on_match, TR tr)
   	const {
        size_t curr = ctx.root;
        size_t idx = 0;
	#if !defined(NDEBUG)
		size_t root_num = initial_state == ctx.root
						? n_words : inline_suffix_cnt(curr);
	#endif
        for (size_t i = ctx.pos; ; ++i) {
			if (this->is_pzip(curr)) {
				fstring zs = this->get_zpath_data(curr, NULL);
				size_t nn = std::min(zs.size(), str.size() - i);
				for (size_t j = ctx.zidx; j < nn; ++j, ++i)
					if ((byte_t)tr(str[i]) != zs[j]) return i;
				if (nn < zs.size())
					return i;
			}
			ctx.zidx = 0;
			if (this->is_term(curr)) {
				on_match(i, base_nth + idx);
				idx++;
				assert(idx <= root_num);
			}
			if (str.size() == i)
				return i;
			const byte_t ch = tr(str[i]);
			size_t n_trans = states[curr].u.s.n_trans + 1;
			if (1 == n_trans && nil_state == states[curr+1].u.e.dest) {
				return i;
			}
			for (size_t j = 0; j < n_trans; ++j) {
				const auto e = states[curr+1+j].u.e;
				if (e.ch < ch) {
					idx += states[e.dest].u.s.n_cnt;
					assert(idx <= root_num);
				}
				else if (e.ch == ch) {
					curr = e.dest;
					goto Found;
				} else
					break;
			}
			return i;
		Found:;
        }
    }

	void nth_word(MatchContext& ctx, size_t nth, std::string* word)
	const override final {
		size_t curr = ctx.root;
		size_t hit = 0;
	//	size_t root_num = initial_state == ctx.root
	//					? n_words : inline_suffix_cnt(curr);
		for (;;) {
			assert(hit <= nth);
			if (this->is_pzip(curr)) {
				fstring zs = this->get_zpath_data(curr, NULL);
				assert(zs.size() > 0);
				word->append(zs.data() + ctx.zidx, zs.size() - ctx.zidx);
				ctx.zidx = 0;
			}
			if (this->is_term(curr)) {
				if (hit == nth)
					break;
				++hit;
			}
			assert(hit <= nth);
			size_t n_trans = states[curr].u.s.n_trans + 1;
			if (1 == n_trans && nil_state == states[curr+1].u.e.dest) {
				abort(); // Unexpected
			}
			for (size_t j = 0; j < n_trans; ++j) {
				auto e = states[curr+1+j].u.e;
				assert(nil_state != e.dest);
				assert(e.dest < states.size());
				size_t hit2 = hit + states[e.dest].u.s.n_cnt;
				if (hit2 <= nth) {
					hit = hit2;
				} else {
					word->push_back(e.ch);
					curr = e.dest;
					break;
				}
			}
		}
	}

	using BaseDAWG::index;
	using BaseDAWG::nth_word;
	using BaseDAWG::match_dawg;
	using BaseDAWG::match_dawg_l;
	const SuffixCountableDAWG* get_SuffixCountableDAWG()
	const override { return this; }

	virtual size_t suffix_cnt(size_t state_id) const override final {
		assert(state_id < states.size());
		return states[state_id].u.s.n_cnt;
	}
	size_t inline_suffix_cnt(size_t state_id) const {
		assert(state_id < states.size());
		return states[state_id].u.s.n_cnt;
   	}

	void dot_write_one_state(FILE* fp, size_t s, const char* ext_attr)
	const override final {
		long ls = s;
		long num = states[ls].u.s.n_cnt;
		if (this->is_pzip(ls)) {
			fstring zs = this->get_zpath_data(ls, NULL);
			const char* s2 = zs.data();
			int n = zs.ilen();
			if (this->is_term(ls))
				fprintf(fp, "\tstate%ld[label=\"%ld:%.*s=%ld\" shape=\"doublecircle\" %s];\n", ls, ls, n, s2, num, ext_attr);
			else
				fprintf(fp, "\tstate%ld[label=\"%ld:%.*s=%ld\" %s];\n", ls, ls, n, s2, num, ext_attr);
		}
		else {
			if (this->is_term(ls))
				fprintf(fp, "\tstate%ld[label=\"%ld=%ld\" shape=\"doublecircle\" %s];\n", ls, ls, num, ext_attr);
			else
				fprintf(fp, "\tstate%ld[label=\"%ld=%ld\" %s];\n", ls, ls, num, ext_attr);
		}
    }

private:
	void compile() {
		assert(!states.empty());
		n_words = 0;
		this->topological_sort(initial_state, [&](size_t parent) {
			assert(parent+1 < states.size());
			size_t pathes = states[parent].u.s.term_bit ? 1 : 0;
			this->for_each_dest(parent, [&](size_t child) {
				assert(child+1 < this->states.size());
				pathes += this->states[child].u.s.n_cnt;
			});
			if (initial_state == parent) {
				this->n_words = pathes;
			}
			if (pathes >= max_cnt_in_state) {
				if (initial_state == parent)
					states[parent].u.s.n_cnt = max_cnt_in_state;
				else
					throw std::out_of_range("LinearDAWG::compile, pathes is too large");
			} else
				states[parent].u.s.n_cnt = pathes;
		});
		assert(0 != n_words);
		is_compiled = true;
	}
	size_t right_language_size(size_t s) const {
		assert(s+1 < states.size());
		return states[s].u.s.n_cnt;
	}
	size_t zpath_slots(size_t s) const {
		size_t zlen = this->get_zpath_data(s, NULL).size();
		return slots_of_bytes(zlen+1);
	}
	static size_t slots_of_bytes(size_t bytes) {
		return (bytes + sizeof(State)-1) / sizeof(State);
	}
	template<class Walker, class Au>
	void build_from_aux(const Au& au, typename Au::state_id_t root) {
		typedef typename Au::state_id_t au_state_id_t;
		valvec<au_state_id_t> map;
		map.resize_no_init(au.total_states());
		size_t idx = 0;
		Walker walker;
		walker.resize(au.total_states());
		walker.putRoot(root);
		while (!walker.is_finished()) {
			au_state_id_t curr = walker.next();
			map[curr] = idx;
			size_t n_child = au.num_children(curr);
			if (au.is_pzip(curr)) {
				size_t zlen = au.get_zpath_data(curr, NULL).size();
				idx += slots_of_bytes(zlen+1);
			}
			idx += 1;
			idx += (0 == n_child) ? 1 : n_child;
			walker.putChildren(&au, curr);
		}
		if (idx >= max_state) {
			string_appender<> msg;
			msg << "LinearDAWG::build_from_aux: idx=" << idx
			   	<< " exceeds max_state=" << max_state;
			throw std::out_of_range(msg);
		}
		states.resize_no_init(idx);
		walker.resize(au.total_states());
		walker.putRoot(root);
		idx = 0;
		while (!walker.is_finished()) {
			au_state_id_t curr = walker.next();
			size_t j = 0;
			assert(idx == map[curr]);
			states[idx].u.s.pzip_bit = au.is_pzip(curr);
			states[idx].u.s.term_bit = au.is_term(curr);
			au.for_each_move(curr,
				[&](au_state_id_t dst, auchar_t ch) {
					assert(dst < map.size());
					assert(idx+1+j < states.size());
					states[idx+1+j].u.e.ch = ch;
					states[idx+1+j].u.e.dest = map[dst];
					j++;
				});
			if (0 == j) {
				states[idx].u.s.n_trans = 0;
				states[idx].u.s.n_cnt = 0;
				states[idx+1].u.e.ch = 0;
				states[idx+1].u.e.dest = nil_state;
				idx += 2;
			} else {
				states[idx].u.s.n_trans = j-1;
				states[idx].u.s.n_cnt = 0;
				idx += j+1;
			}
			if (au.is_pzip(curr)) {

				#if defined(__clang__)
					#pragma clang diagnostic push
					#pragma clang diagnostic ignored "-Wstringop-overflow="
				#elif defined(__GNUC__)
					#pragma GCC diagnostic push
					#pragma GCC diagnostic ignored "-Wstringop-overflow="
				#endif // Fuck GCC LTO

				const unsigned char* zd = au.get_zpath_data(curr, NULL).udata()-1;
				int zlen = *zd;
				size_t zp_slots = slots_of_bytes(zlen+1);
				memcpy(states[idx].u.z.data, zd, zlen+1);
				idx += zp_slots;

				#if defined(__clang__)
					#pragma clang diagnostic pop
				#elif defined(__GNUC__)
					#pragma GCC diagnostic pop
				#endif // Fuck GCC LTO
			}
			walker.putChildren(&au, curr);
		}
		m_zpath_states = au.num_zpath_states();
		assert(states.size() == idx);
		compile();
	}
public:
	enum { SERIALIZATION_VERSION = 2 };
	template<class DataIO> void load_au(DataIO& dio, size_t version) {
		typename DataIO::my_var_size_t stateBytes, n_states, n_zpath_states;
		typename DataIO::my_var_size_t n_words2;
		dio >> stateBytes;
		dio >> n_zpath_states;
		dio >> n_states;
		dio >> n_words2;
		if (version >= 2) {
			load_kv_delim_and_is_dag(this, dio);
		}
		this->m_is_dag = true; // DAWG is always DAG
		if (sizeof(State) != stateBytes.t) {
			typedef typename DataIO::my_DataFormatException bad_format;
			string_appender<> msg;
			msg << "LinearDAWG::load_au: StateBytes[";
			msg << "data=" << stateBytes << " ";
			msg << "code=" << sizeof(State) << "]";
			throw bad_format(msg);
		}
		states.resize_no_init(n_states.t);
		dio.ensureRead(states.data(), sizeof(State) * n_states.t);
		m_zpath_states = size_t(n_zpath_states.t);
		n_words = n_words2;
		is_compiled = true;
	}
	template<class DataIO> void save_au(DataIO& dio) const {
		dio << typename DataIO::my_var_size_t(sizeof(State));
		dio << typename DataIO::my_var_size_t(m_zpath_states);
		dio << typename DataIO::my_var_size_t(states.size());
		dio << typename DataIO::my_var_size_t(n_words);
		dio << uint16_t(this->m_kv_delim); // version >= 2
		dio << uint08_t(1); // version >= 2
		dio.ensureWrite(states.data(), sizeof(State) * states.size());
	}
	template<class DataIO>
	friend void DataIO_loadObject(DataIO& dio, LinearDAWG& au) {
		typename DataIO::my_var_size_t version;
		dio >> version;
		if (version > SERIALIZATION_VERSION) {
			typedef typename DataIO::my_BadVersionException bad_ver;
			throw bad_ver(version.t, SERIALIZATION_VERSION, "LinearDAWG");
		}
		au.load_au(dio, version.t);
	}
	template<class DataIO>
	friend void DataIO_saveObject(DataIO& dio, const LinearDAWG& au) {
		dio << typename DataIO::my_var_size_t(SERIALIZATION_VERSION);
		au.save_au(dio);
	}

	~LinearDAWG() {
		if (this->mmap_base) {
			states.risk_release_ownership();
		}
	}

	void finish_load_mmap(const DFA_MmapHeader* base) override {
		assert(sizeof(State) == base->state_size);
		byte_t* bbase = (byte_t*)base;
		states.clear();
		states.risk_set_data((State*)(bbase + base->blocks[0].offset));
		states.risk_set_size(size_t(base->total_states));
		states.risk_set_capacity(size_t(base->total_states));
		this->is_compiled = true;
		this->n_words = size_t(base->dawg_num_words);
	}

	long prepare_save_mmap(DFA_MmapHeader* base, const void** dataPtrs)
	const override {
		base->is_dag = true;
		base->state_size = sizeof(State);
		base->num_blocks = 1;
		base->blocks[0].offset = sizeof(DFA_MmapHeader);
		base->blocks[0].length = sizeof(State)*states.size();
		dataPtrs[0] = states.data();
		base->dawg_num_words = this->n_words;

		return 0;
	}

protected:
	valvec<State> states;
	size_t m_gnode_states;
	bool  is_compiled;

typedef LinearDAWG MyType;
//#include "ppi/for_each_suffix.hpp"
//#include "ppi/match_key.hpp"
//#include "ppi/match_path.hpp"
//#include "ppi/match_prefix.hpp"
#include "ppi/dfa_const_virtuals.hpp"
#include "ppi/dawg_const_virtuals.hpp"
#include "ppi/post_order.hpp"
#include "ppi/virtual_suffix_cnt.hpp"
#include "ppi/state_move_fast.hpp"
#include "ppi/adfa_iter.hpp"
#include "ppi/for_each_word.hpp"
};

} // namespace terark

#pragma once

#include <assert.h>
#include <stdio.h>

#include <algorithm>
#include <typeinfo>
#include <terark/num_to_str.hpp>
#include <terark/mempool_lock_none.hpp>

#include "dfa_algo.hpp"
#include "dfa_mmap_header.hpp"
#include "graph_walker.hpp"

namespace terark {

#define TERARK_PP_Identity(x) x
#define TERARK_PP_Arg0(a0,a1,a2,a3,a4) a0
#define TERARK_PP_Arg1(a0,a1,a2,a3,a4) a1
#define TERARK_PP_Arg2(a0,a1,a2,a3,a4) a2
#define TERARK_PP_Arg3(a0,a1,a2,a3,a4) a3
#define TERARK_PP_Arg4(a0,a1,a2,a3,a4) a4

#pragma pack(push,1) // for all #include "ppi/packed_state.hpp"

// When MyBytes is 6, intentional waste a bit: sbit is 32, nbm is 5
// When sbits==34, sizeof(PackedState)==6, StateID is uint64, the State34

#define ClassName_StateID_PtrBits_nbmBits_Sigma (State4B,uint32_t,18,4,256)
#include "ppi/packed_state.hpp" ///< 4 byte per-state, max_state is 256K-1

#define ClassName_StateID_PtrBits_nbmBits_Sigma (State5B,uint32_t,26,4,256)
#include "ppi/packed_state.hpp" ///< 5 byte per-state, max_state is  64M-1

#define ClassName_StateID_PtrBits_nbmBits_Sigma (State6B,uint32_t,32,6,256)
#include "ppi/packed_state.hpp" ///< 6 byte per-state, max_state is   4G-1

BOOST_STATIC_ASSERT(sizeof(State4B) == 4);
BOOST_STATIC_ASSERT(sizeof(State5B) == 5);
BOOST_STATIC_ASSERT(sizeof(State6B) == 6);

#if !defined(_MSC_VER)
#if TERARK_WORD_BITS >= 64 && defined(TERARK_INST_ALL_DFA_TYPES)
#define ClassName_StateID_PtrBits_nbmBits_Sigma (State34,uint64_t,34,4,256)
#include "ppi/packed_state.hpp" ///< 6 byte per-state, max_state is  16G-1
BOOST_STATIC_ASSERT(sizeof(State34) == 6);

#define ClassName_StateID_PtrBits_nbmBits_Sigma (State7B,uint64_t,42,4,256)
#include "ppi/packed_state.hpp" ///< 7 byte per-state, max_state is   4T-1
BOOST_STATIC_ASSERT(sizeof(State7B) == 7);
#endif
#endif

#define ClassName_StateID_PtrBits_nbmBits_Sigma (State4B_448,uint32_t,17,4,448)
#include "ppi/packed_state.hpp"  ///< 4 byte per-state, max_state is 128K-1
BOOST_STATIC_ASSERT(sizeof(State4B_448) == 4);

#define ClassName_StateID_PtrBits_nbmBits_Sigma (State6B_512,uint32_t,32,5,512)
#include "ppi/packed_state.hpp" ///< 6 byte per-state, max_state is   4G-1
BOOST_STATIC_ASSERT(sizeof(State6B_512) == 6);

#if !defined(_MSC_VER)
#define ClassName_StateID_PtrBits_nbmBits_Sigma (State5B_448,uint32_t,25,4,448)
#include "ppi/packed_state.hpp" ///< 5 byte per-state, max_state is  32M-1
BOOST_STATIC_ASSERT(sizeof(State5B_448) == 5);
#if TERARK_WORD_BITS >= 64 && defined(TERARK_INST_ALL_DFA_TYPES)
#define ClassName_StateID_PtrBits_nbmBits_Sigma (State7B_448,uint64_t,41,4,448)
#include "ppi/packed_state.hpp" ///< 7 byte per-state, max_state is   2T-1
BOOST_STATIC_ASSERT(sizeof(State7B_448) == 7);
#endif
#endif

#pragma pack(pop) // end for all #include "ppi/packed_state.hpp"

#define TERARK_PP_sizeof_uint32_t 4
#define TERARK_PP_sizeof_uint64_t 8

#define ClassName_StateID_Sigma (State32_256,uint32_t,256,~,~)
#include "ppi/fast_state.hpp"
typedef State32_256  State32;
#define ClassName_StateID_Sigma (State32_512,uint32_t,512,~,~)
#include "ppi/fast_state.hpp"

#if TERARK_WORD_BITS >= 64
	#define ClassName_StateID_Sigma (State64_256,uint64_t,256,~,~)
	#include "ppi/fast_state.hpp"
	typedef State64_256  State64;
	#define ClassName_StateID_Sigma (State64_512,uint64_t,512,~,~)
	#include "ppi/fast_state.hpp"
#endif

/// State must has trivial destructor
template<class State, class SuperDFA>
class AutomataTpl : public SuperDFA, public DFA_MutationInterface {
protected:
	enum { MemPool_Align = State::MemPool_Align };
	typedef typename State::transition_t Transition;
    typedef MemPool_LockNone<MemPool_Align> MyMemPool;
public:
	typedef State      state_t;
	typedef typename State::transition_t transition_t;
    typedef typename State::position_t position_t;
    typedef typename State::state_id_t state_id_t;
    static const int edge_size = sizeof(Transition);
	static const bool has_register = 0;
	enum { sigma = State::sigma };
    static const state_id_t max_state = State::max_state;
    static const state_id_t nil_state = State::nil_state;

    struct header_b32 {
        uint32_t   b;
        Transition s[1];
    };
    typedef bm_uint_t big_bm_b_t;
    struct header_max : public static_bitmap<sigma, big_bm_b_t> {
        const Transition* base(int bits) const {
            assert(bits <= this->TotalBits);
            int n = (bits + this->BlockBits-1) / this->BlockBits;
            return (const Transition*)(&this->bm[n]);
        }
        Transition* base(int bits) {
            assert(bits <= this->TotalBits);
            int n = (bits + this->BlockBits-1) / this->BlockBits;
            return (Transition*)(&this->bm[n]);
        }
    };

    static int bm_real_bits(int n) {
        if (is_32bitmap(n))
            return 32;
        else // align to blockBits boundary
            return header_max::align_bits(n);
    }

	static bool is_32bitmap(int bits) {
		assert(bits >= 2);
		return sizeof(Transition) <= 4 &&
		       sizeof(big_bm_b_t) >  4 &&
			   bits <= 32;
	}

	bool has_freelist() const override final { return true; }

private:
    void insert(State& s, Transition val,
                int oldbits, int newbits, int inspos, int oldouts);

    void check_slots(State s, int n);
	void init();

public:

    AutomataTpl() : pool(sizeof(Transition)*sigma + sigma/8) { init(); }
    void reserve(size_t reserved_state_num);
	void clear() { states.clear(); pool.clear(); init(); }
	void erase_all() { states.erase_all(); pool.erase_all(); init(); }

	const valvec<State>& internal_get_states() const { return states; }
	const MyMemPool    & internal_get_pool  () const { return pool  ; }
    size_t total_states() const { return states.size(); }
    size_t total_transitions() const { return transition_num; }
    size_t new_state() override;
    size_t clone_state(size_t source) override final;
    void del_state(size_t s) override final;
	void del_all_move(size_t s) override final;
	size_t min_free_state() const;
	size_t num_free_states() const { return numFreeStates; }
	size_t num_used_states() const { return states.size() - numFreeStates; }
	size_t v_gnode_states() const final { return num_used_states(); }

    bool is_free(state_id_t s) const {
        assert(size_t(s) < states.size());
        return states[s].is_free();
    }
	bool is_pzip(state_id_t s) const {
		return states[s].is_pzip();
	}
    bool is_term(state_id_t s) const {
        assert(s < (state_id_t)states.size());
        return states[s].is_term();
    }
    void set_term_bit(state_id_t s) {
        assert(s < (state_id_t)states.size());
        states[s].set_term_bit();
    }
    void clear_term_bit(state_id_t s) {
        assert(s < (state_id_t)states.size());
        states[s].clear_term_bit();
    }
	bool v_has_children(size_t s) const override final {
		assert(s < states.size());
		return states[s].has_children();
	}
	bool has_children(state_id_t s) const {
		assert(s < (state_id_t)states.size());
		return states[s].has_children();
	}
	bool more_than_one_child(state_id_t s) const {
        assert(s < (state_id_t)states.size());
		return states[s].more_than_one_child();
	}
	void set_single_child(state_id_t parent, state_id_t child) {
		assert(0 == m_zpath_states);
        assert(parent < (state_id_t)states.size());
        assert(child  < (state_id_t)states.size());
		states[parent].set_target(child);
	}
	state_id_t get_single_child(state_id_t parent) const {
		assert(0 == m_zpath_states);
        assert(parent < (state_id_t)states.size());
		assert(!states[parent].more_than_one_child());
		return states[parent].get_target();
	}
	auchar_t get_single_child_char(state_id_t parent) const {
		assert(0 == m_zpath_states);
        assert(parent < (state_id_t)states.size());
		assert(!states[parent].more_than_one_child());
		return states[parent].getMinch();
	}

    Transition state_move(state_id_t curr, auchar_t ch) const {
        assert(curr < (state_id_t)states.size());
		ASSERT_isNotFree(curr);
        return state_move(states[curr], ch);
	}
    Transition state_move(const State& s, auchar_t ch) const;

	using DFA_MutationInterface::add_all_move;
	void add_all_move(size_t s, const CharTarget<size_t>* trans, size_t n)
	override final;

protected:
    size_t
	add_move_imp(size_t source, size_t target, auchar_t ch, bool OverwriteExisted)
	override final;
public:
	size_t v_num_children(size_t s) const override { return num_children(s); }
	size_t num_children(size_t curr) const;
	size_t trans_bytes(state_id_t sid) const {
		return trans_bytes(states[sid]);
	}
	size_t trans_bytes(const State& s) const;

	state_id_t single_target(state_id_t s) const { return single_target(states[s]); }
	state_id_t single_target(const State& s) const {
		// just 1 trans, if not ziped, needn't extra memory in pool
		// default single_target is nil_state, just the designated
		return s.is_pzip() ? pool.template at<state_id_t>(s.getpos()) : s.get_target();
	}

    // just walk the (curr, dest), don't access the 'ch'
    // this is useful such as when minimizing DFA
    template<class OP> // use ctz (count trailing zero)
    void for_each_dest(state_id_t curr, OP op) const {
        const State& s = states[curr];
        if (!s.more_than_one_child()) {
            state_id_t target = single_target(s);
            if (nil_state != target)
                op(State::first_trans(target));
            return;
        }
        int bits = s.rlen();
        int tn;
        const Transition* tp;
        if (is_32bitmap(bits)) {
            const header_b32& hb = pool.template at<header_b32>(s.getpos());
            tn = fast_popcount32(hb.b);
            tp = hb.s;
        } else {
            const header_max& hb = pool.template at<header_max>(s.getpos());
            tn = hb.popcnt_aligned(bits);
            tp = hb.base(bits);
        }
        for (int i = 0; i < tn; ++i)
            op(tp[i]);
    }

	// for_each_dest in reversed order
    template<class OP>
    void for_each_dest_rev(state_id_t curr, OP op) const {
        const State& s = states[curr];
        if (!s.more_than_one_child()) {
            state_id_t target = single_target(s);
            if (nil_state != target)
                op(State::first_trans(target));
            return;
        }
        int bits = s.rlen();
        int tn;
        const Transition* tp;
        if (is_32bitmap(bits)) {
            const header_b32& hb = pool.template at<header_b32>(s.getpos());
            tn = fast_popcount32(hb.b);
            tp = hb.s;
        } else {
            const header_max& hb = pool.template at<header_max>(s.getpos());
            tn = hb.popcnt_aligned(bits);
            tp = hb.base(bits);
        }
        for (int i = tn-1; i >= 0; --i)
            op(tp[i]);
    }

    template<class OP> // use ctz (count trailing zero)
    void for_each_move(state_id_t curr, OP op) const {
		ASSERT_isNotFree(curr);
        const State s = states[curr];
        if (!s.more_than_one_child()) {
			state_id_t target = single_target(s);
			if (nil_state != target)
				op(State::first_trans(target), s.getMinch());
			return;
		}
        const int bits = s.rlen();
        if (is_32bitmap(bits)) {
            const header_b32& hb = pool.template at<header_b32>(s.getpos());
            uint32_t bm = hb.b;
            auchar_t ch = s.getMinch();
            for (int pos = 0; bm; ++pos) {
                const Transition& target = hb.s[pos];
                int ctz = fast_ctz32(bm);
                ch += ctz;
                op(target, ch);
                ch++;
                bm >>= ctz; // must not be bm >>= ctz + 1
                bm >>= 1;   // because ctz + 1 may be bits of bm(32 or 64)
            }
        } else {
            const header_max& hb = pool.template at<header_max>(s.getpos());
            const Transition* base = hb.base(bits);
            const int cBlock = hb.align_bits(bits) / hb.BlockBits;
            for (int pos = 0, i = 0; i < cBlock; ++i) {
                big_bm_b_t bm = hb.block(i);
                auchar_t ch = s.getMinch() + i * hb.BlockBits;
                for (; bm; ++pos) {
                    const Transition& target = base[pos];
                    int ctz = fast_ctz(bm);
                    ch += ctz;
                    op(target, ch);
                    ch++;
                    bm >>= ctz; // must not be bm >>= ctz + 1
                    bm >>= 1;   // because ctz + 1 may be bits of bm(32 or 64)
                    assert(pos < hb.BlockBits * cBlock);
                }
            }
        }
    }

	// exit immediately if op returns false
    template<class OP> // use ctz (count trailing zero)
    void for_each_move_break(state_id_t curr, OP op) const {
		ASSERT_isNotFree(curr);
        const State s = states[curr];
        if (!s.more_than_one_child()) {
			state_id_t target = single_target(s);
			if (nil_state != target)
				op(State::first_trans(target), s.getMinch());
			return;
		}
        const int bits = s.rlen();
        if (is_32bitmap(bits)) {
            const header_b32& hb = pool.template at<header_b32>(s.getpos());
            uint32_t bm = hb.b;
            auchar_t ch = s.getMinch();
            for (int pos = 0; bm; ++pos) {
                const Transition& target = hb.s[pos];
                int ctz = fast_ctz32(bm);
                ch += ctz;
                if (!op(target, ch))
					return;
                ch++;
                bm >>= ctz; // must not be bm >>= ctz + 1
                bm >>= 1;   // because ctz + 1 may be bits of bm(32 or 64)
            }
        } else {
            const header_max& hb = pool.template at<header_max>(s.getpos());
            const Transition* base = hb.base(bits);
            const int cBlock = hb.align_bits(bits) / hb.BlockBits;
            for (int pos = 0, i = 0; i < cBlock; ++i) {
                big_bm_b_t bm = hb.block(i);
                auchar_t ch = s.getMinch() + i * hb.BlockBits;
                for (; bm; ++pos) {
                    const Transition& target = base[pos];
                    int ctz = fast_ctz(bm);
                    ch += ctz;
                    if (!op(target, ch))
						return;
                    ch++;
                    bm >>= ctz; // must not be bm >>= ctz + 1
                    bm >>= 1;   // because ctz + 1 may be bits of bm(32 or 64)
                    assert(pos < hb.BlockBits * cBlock);
                }
            }
        }
    }

    template<class OP>
    void for_each_move_slow(state_id_t curr, OP op) const {
		ASSERT_isNotFree(curr);
        const State s = states[curr];
        if (!s.more_than_one_child()) {
            state_id_t target = single_target(s);
            if (nil_state != target)
                op(State::first_trans(target), s.getMinch());
			return;
        }
        const int bits = s.rlen();
        if (is_32bitmap(bits)) {
            const header_b32& hb = pool.template at<header_b32>(s.getpos());
            uint32_t bm = hb.b;
            auchar_t ch = s.getMinch();
            for (int pos = 0; bm; ++ch) {
                if (bm & 1) {
                    const Transition& target = hb.s[pos];
                    op(target, ch);
                    pos++;
                }
                bm >>= 1;
            }
        } else {
            const header_max& hb = pool.template at<header_max>(s.getpos());
            const state_id_t* base = hb.base(bits);
            const int cBlock = hb.align_bits(bits) / hb.BlockBits;
            for (int pos = 0, i = 0; i < cBlock; ++i) {
                big_bm_b_t bm = hb.block(i);
                // could use intrinsic ctz(count of trailing zero)
                // to reduce time complexity
                auchar_t ch = s.getMinch() + i * hb.BlockBits;
                for (; bm; ++ch) {
                    if (bm & 1) {
                        const Transition& target = base[pos];
                        op(target, ch);
                        if (ch >= s.getMaxch()) // additional optimization
                            return;
                        pos++;
                    }
                    bm >>= 1;
                }
            }
        }
    }

    void resize_states(size_t new_states_num) override final;
    size_t mem_size() const override;
	size_t frag_size() const { return pool.frag_size(); }
	void shrink_to_fit();
	void compact() {}
    void swap(AutomataTpl& y);
    void risk_swap(AutomataTpl& y);
    void add_zpath(state_id_t ss, const byte_t* str, size_t len);
	size_t get_zipped_path_str_pos(const State& s) const;

public:
	fstring get_zpath_data(const State& s) const;
	fstring get_zpath_data(size_t s, MatchContext*) const;

	enum { SERIALIZATION_VERSION = 4 };

	template<class DataIO> void load_au(DataIO& dio, size_t version) {
		typename DataIO::my_var_size_t n_pool, n_states;
		typename DataIO::my_var_size_t n_firstFreeState, n_numFreeStates;
		typename DataIO::my_var_size_t n_zpath_states;
		dio >> n_zpath_states;
		dio >> n_pool; // keeped but not used in version=2
		dio >> n_states;
		dio >> n_firstFreeState;
		dio >> n_numFreeStates;
		if (version >= 4)
			load_kv_delim_and_is_dag(this, dio);
		m_zpath_states = n_zpath_states;
		states.resize_no_init(n_states.t);
		if (1 == version) {
			pool.resize_no_init(n_pool.t);
			if (pool.size())
				dio.ensureRead(&pool.template at<byte_t>(0), n_pool.t);
		}
		else {
			if (version > SERIALIZATION_VERSION) {
				typedef typename DataIO::my_BadVersionException bad_ver;
				throw bad_ver(version, SERIALIZATION_VERSION, "AutomataTpl");
			}
			dio >> pool;
		}
		dio.ensureRead(&states[0], sizeof(State)*n_states.t);
		if (version < 3 && m_zpath_states) {
			m_zpath_states = 0;
			for (size_t i = 0; i < n_states.t; ++i) {
				if (!states[i].is_free() && states[i].is_pzip())
					m_zpath_states++;
			}
		}
		if (version < 4) {
			// this->m_is_dag = this->tpl_is_dag();
			// m_kv_delim is absent in version < 4
		}
	}
	template<class DataIO> void save_au(DataIO& dio) const {
		dio << typename DataIO::my_var_uint64_t(m_zpath_states);
		dio << typename DataIO::my_var_uint64_t(pool  .size());
		dio << typename DataIO::my_var_uint64_t(states.size());
		dio << typename DataIO::my_var_uint64_t(firstFreeState);
		dio << typename DataIO::my_var_uint64_t(numFreeStates);
		dio << uint16_t(this->m_kv_delim); // version >= 4
		dio << char(this->m_is_dag); // version >= 4
		dio << pool; // version=2
		dio.ensureWrite(states.data(), sizeof(State) * states.size());
	}

	template<class DataIO>
	friend void DataIO_loadObject(DataIO& dio, AutomataTpl& au) {
		typename DataIO::my_var_size_t version;
		dio >> version;
		if (version > SERIALIZATION_VERSION) {
			typedef typename DataIO::my_BadVersionException bad_ver;
			throw bad_ver(version.t, SERIALIZATION_VERSION, "AutomataTpl");
		}
		std::string state_name;
		dio >> state_name;
#if 0 // don't check
	  // but keep load state_name for data format compatible
	  // by using BaseDFA::load_from, check is unneeded
		if (typeid(State).name() != state_name) {
			if (1 != version.t) { // strong check for new version
				typedef typename DataIO::my_DataFormatException bad_format;
				string_appender<> msg;
				msg << "state_name[data=" << state_name
					<< " @ code=" << typeid(State).name()
					<< "]";
				throw bad_format(msg);
			}
		}
#endif
		au.load_au(dio, version.t);
	}

	template<class DataIO>
	friend void DataIO_saveObject(DataIO& dio, const AutomataTpl& au) {
		dio << typename DataIO::my_var_uint64_t(SERIALIZATION_VERSION);
		dio << fstring(typeid(State).name());
		au.save_au(dio);
	}

//	size_t mempool_meta_size() const;

	~AutomataTpl();

	size_t m_atom_dfa_num;
	size_t m_dfa_cluster_num;

protected:
	MyMemPool pool;
    valvec<State> states;
    size_t firstFreeState;
    size_t numFreeStates;
    size_t transition_num;
	using SuperDFA::m_zpath_states;
	using SuperDFA::m_total_zpath_len;

	friend size_t
	GraphTotalVertexes(const AutomataTpl* au) { return au->states.size(); }
public:
	size_t hopcroft_hash(size_t x_id) const;
	size_t onfly_hash_hash(size_t x_id) const;
	bool onfly_hash_equal(size_t x_id, size_t y_id) const;
	size_t adfa_hash_hash(const state_id_t* Min, size_t state_id) const;
	bool adfa_hash_equal(const state_id_t* Min, size_t x_id, size_t y_id) const;
	void give_to(AutomataTpl* au2) { this->risk_swap(*au2); clear(); }

	typedef AutomataTpl MyType;
#include "ppi/pool_dfa_mmap.hpp"
};

template<class State>
class AutomataAsBaseDFA : public AutomataTpl<State, BaseDFA> {
	typedef AutomataTpl<State, BaseDFA> super;
	typedef AutomataAsBaseDFA MyType;
public:
#include "ppi/dfa_using_template_base.hpp"
#include "ppi/dfa_const_methods_use_walk.hpp"
#include "ppi/dfa_const_virtuals.hpp"
#include "ppi/dfa_mutation_virtuals_basic.hpp"
};

} // namespace terark

#include "automata_tpl_impl.hpp"


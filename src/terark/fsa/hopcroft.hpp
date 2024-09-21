#pragma once

#include <stddef.h>
#include <stdio.h>
#include <terark/valvec.hpp>
#include <terark/smallmap.hpp>
#include <terark/hash_common.hpp>
#include "graph_walker.hpp"

namespace terark {

#ifdef TRACE_AUTOMATA
    #define AU_TRACE printf
#else
//  #define AU_TRACE 1 ? 0 : printf
    #define AU_TRACE(format, ...)
#endif

template<class DFA>
struct HopcroftDeltaHash {
	size_t operator()(const DFA& dfa, size_t state) const {
		return dfa.hopcroft_hash(state);
	}
};
TERARK_DLL_EXPORT bool HopcroftUseHash();

template<class StateID>
struct Hopcroft {
	typedef StateID BlockID; // also served as blid block_id
	struct Elem {
		StateID  next;
		StateID  prev;
		BlockID  blid;
	};
	struct Block {
		StateID  head;
//		StateID  blen; // block length, use integer type StateID
		StateID  blen : sizeof(StateID)*8-1;
		unsigned isInW: 1;
		Block() : head(-1), blen(0), isInW(0) {}
	};
	const static StateID max_blen = StateID(StateID(1) << (sizeof(StateID)*8-1)) - 1;
    struct Splitter : valvec<StateID> {
        explicit Splitter() {
            this->ch = -1;
            this->reserve(32);
        }
        void sort() {
            assert(-1 != ch);
            std::sort(this->begin(), this->end());
        }
        void resize0() {
            this->ch = -1;
            this->resize(0);
            if (this->capacity() > 8*1024)
                this->shrink_to_fit();
        }
        void insert(StateID x) { this->push_back(x); }
        ptrdiff_t ch; // for better alignment
    };
	valvec<Elem>  L; // parallel with Automata::states
	valvec<Block> P; // partition

	// This constructor could be used to minimize multiple DFA in one
	// DFA object
	template<class DFA>
	explicit Hopcroft(const DFA& dfa) {
		BOOST_STATIC_ASSERT(sizeof(StateID) >= sizeof(typename DFA::state_id_t));
		assert(dfa.has_freelist());
		assert(dfa.total_states() >= 1);
		L.resize_no_init(dfa.total_states());
		P.reserve((dfa.total_states()+1)/2);
		P.resize(2);
		bool HasFreeStates = (dfa.num_free_states() != 0);
		for (size_t s = 0; s < L.size(); ++s) {
			if (HasFreeStates && dfa.is_free(s)) continue;
			BlockID blid = dfa.is_term(s) ? 1 : 0;
			if (P[blid].blen == max_blen)
				throw std::length_error(BOOST_CURRENT_FUNCTION);
			add(blid, s);
		}
		if (0 == P[0].blen) {
			std::swap(P[0], P[1]);
			fix_blid(0);
		}
		if (0 == P[1].blen) P.pop_back();
	}
	template<class DFA, class DeltaHash>
	explicit Hopcroft(const DFA& dfa, DeltaHash hash) {
		BOOST_STATIC_ASSERT(sizeof(StateID) >= sizeof(typename DFA::state_id_t));
		assert(dfa.has_freelist());
		assert(dfa.total_states() >= 1);
		L.resize_no_init(dfa.total_states());
		P.reserve((dfa.total_states()+1)/2);
		constexpr StateID nil = StateID(-1);
		const size_t n_buckets = __hsm_stl_next_prime(dfa.total_states() + 1);
		AutoFree<StateID> bucket(n_buckets, nil);
		bool HasFreeStates = (dfa.num_free_states() != 0);
		for (size_t s = 0; s < L.size(); ++s) {
			if (HasFreeStates && dfa.is_free(s)) continue;
			StateID hidx = hash(dfa, s) % n_buckets;
			StateID blid = bucket[hidx];
			if (nil == blid) { // make a new block
				blid = P.size();
				P.push_back();
				// do not consider hash equal but <value> does not equal
				bucket[hidx] = blid; // insert to hash hidx
			}
			else if (P[blid].blen == max_blen)
				throw std::length_error(BOOST_CURRENT_FUNCTION);
			add(blid, s);
		}
	}

	template<class DFA>
	Hopcroft(const DFA& dfa, StateID RootState) {
		assert(dfa.total_states() >= 1);
		slow_init(dfa, &RootState, 1);
	}
	template<class DFA, class Uint>
	Hopcroft(const DFA& dfa, const Uint* roots, size_t n_roots) {
		assert(dfa.total_states() >= 1);
		slow_init(dfa, roots, n_roots);
	}

	template<class DFA, class Uint>
	void slow_init(const DFA& dfa, const Uint* roots, size_t n_roots) {
		BOOST_STATIC_ASSERT(sizeof(StateID) >= sizeof(typename DFA::state_id_t));
		L.resize_no_init(dfa.total_states());
		P.reserve((dfa.total_states()+1)/2);
		P.resize(2);
		PFS_GraphWalker<StateID> walker;
		walker.resize(dfa.total_states());
		for (size_t i = 0; i < n_roots; ++i) walker.putRoot(roots[i]);
		while (!walker.is_finished()) {
			StateID curr = walker.next();
			BlockID blid = dfa.is_term(curr) ? 1 : 0;
			if (P[blid].blen == max_blen)
				throw std::length_error(BOOST_CURRENT_FUNCTION);
			add(blid, curr);
			walker.putChildren(&dfa, curr);
		}
		if (0 == P[0].blen) {
			std::swap(P[0], P[1]);
			fix_blid(0);
		}
		if (0 == P[1].blen) P.pop_back();
	}

	template<class DFA, class DeltaHash>
	Hopcroft(const DFA& dfa, StateID RootState, DeltaHash hash) {
		assert(dfa.total_states() >= 1);
		init_with_hash(dfa, &RootState, 1, hash);
	}
	template<class DFA, class Uint, class DeltaHash>
	Hopcroft(const DFA& dfa, const Uint* roots, size_t n_roots,
			 DeltaHash hash) {
		assert(dfa.total_states() >= 1);
		init_with_hash(dfa, roots, n_roots, hash);
	}
	template<class DFA, class Uint, class DeltaHash>
	void init_with_hash(const DFA& dfa, const Uint* roots, size_t n_roots,
						DeltaHash hash) {
		constexpr StateID nil = StateID(-1);
		const size_t n_buckets = __hsm_stl_next_prime(dfa.total_states() + 1);
		AutoFree<StateID> bucket(n_buckets, nil);
		assert(dfa.total_states() >= 1);
		BOOST_STATIC_ASSERT(sizeof(StateID) >= sizeof(typename DFA::state_id_t));
		L.resize_no_init(dfa.total_states());
		P.reserve((dfa.total_states()+1)/2);
		assert(P.empty());
		PFS_GraphWalker<StateID> walker;
		walker.resize(dfa.total_states());
		for (size_t i = 0; i < n_roots; ++i) walker.putRoot(roots[i]);
		while (!walker.is_finished()) {
			StateID curr = walker.next();
			StateID hidx = hash(dfa, curr) % n_buckets;
			StateID blid = bucket[hidx];
			if (nil == blid) { // make a new block
				blid = P.size();
				P.push_back();
				// do not consider hash equal but <value> does not equal
				bucket[hidx] = blid; // insert to hash hidx
			}
			else if (P[blid].blen == max_blen) {
				throw std::length_error(BOOST_CURRENT_FUNCTION);
			}
			add(blid, curr); // reuse L as hash link
			walker.putChildren(&dfa, curr);
		}
	}

	// add 's' to end of P[g], P[g] is a double linked cyclic list
	void add(BlockID g, StateID s) {
		if (P[g].blen == 0) {
			P[g].head = s;
			L[s].next = s;
			L[s].prev = s;
		} else {
			StateID h = P[g].head;
			StateID t = L[h].prev;
        //  assert(h <= t);
		//	assert(s > t);
			L[s].next = h;
			L[s].prev = t;
			L[h].prev = s;
			L[t].next = s;
		}
		P[g].blen++;
		L[s].blid = g;
	}
	void del(BlockID g, StateID s) {
		assert(L[s].blid == g);
		assert(P[g].blen >= 1);
    //  assert(P[g].head <= s);
		if (P[g].head == s)
			P[g].head = L[s].next;
		P[g].blen--;
		L[L[s].prev].next = L[s].next;
		L[L[s].next].prev = L[s].prev;
	}
    void printSplitter(const char* sub, const Splitter& z) {
		TERARK_UNUSED_VAR(sub);
		TERARK_UNUSED_VAR(z);
#ifdef TRACE_AUTOMATA
        printf("%s[%c]: ", sub, z.ch);
        for (size_t i = 0; i < z.size(); ++i) {
            printf("(%ld %ld)", (long)z[i], (long)L[z[i]].blid);
        }
        printf("\n");
#endif
    }
    void printMappings() {
#ifdef TRACE_AUTOMATA
        for (BlockID b = 0; b < P.size(); ++b) {
            const Block B = P[b];
            StateID s = B.head;
            printf("Block:%02ld: ", (long)b);
            do {
                printf("%ld ", (long)s);
                s = L[s].next;
            } while (B.head != s);
            printf("\n");
        }
#endif
    }
	void printBlock(BlockID blid) const {
#ifdef TRACE_AUTOMATA
		const BlockID head = P[blid].head;
		StateID s = head;
		printf("    block[%ld]:", (long)blid);
		do {
			printf(" %ld", (long)s);
			s = L[s].next;
		} while (head != s);
		printf("\n");
#endif
	}

	BlockID minBlock(BlockID b1, BlockID b2) const {
		if (P[b1].blen < P[b2].blen)
			return b1;
		else
			return b2;
	}

	template<class E_Gen> void refine(E_Gen egen) {
		assert(P.size() >= 1);
#ifdef WAITING_SET_FIFO
		struct WaitingSet : std::deque<BlockID> {};
#else
		struct WaitingSet : valvec<BlockID> {};
#endif
		WaitingSet   W;
#define putW(b) do{BlockID b2 = b; W.push_back(b2); P[b2].isInW = 1;}while(0)
#if defined(_MSC_VER)
		auto takeW = [&]() -> BlockID {
	#ifdef WAITING_SET_FIFO
			BlockID b = W.front();
			W.pop_front();
	#else
			BlockID b = W.back();
			W.pop_back();
	#endif
			P[b].isInW = 0;
			return b;
		};
#else
	#define takeW() ({BlockID b = W.back(); W.pop_back(); P[b].isInW = 0; b;})
#endif
	smallmap<Splitter> C_1(512); // set of p which move(p,ch) is in Block C
	valvec<BlockID> bls; // candidate splitable Blocks
	{// Init WaitingSet
		for (BlockID b = 0; b < (BlockID)P.size(); ++b)
			putW(b);
	}
	long iterations = 0;
	long num_split = 0;
	long non_split_fast = 0;
	long non_split_slow = 0;
	while (!W.empty()) {
		const BlockID C = takeW();
		{
			C_1.resize0();
			StateID head = P[C].head;
			StateID curr = head;
			do {
				egen(curr, C_1);
				curr = L[curr].next;
			} while (curr != head);
		}
AU_TRACE("iteration=%03ld, size[P=%ld, W=%ld, C_1=%ld], C=%ld\n"
        , iterations, (long)P.size(), (long)W.size(), (long)C_1.size()
        , (long)C);
//-split----------------------------------------------------------------------
for (size_t j = 0; j < C_1.size(); ++j) {
	Splitter& E = C_1.byidx(j);
	assert(!E.empty());
//  E.sort();
    size_t Esize = E.size();
{//----Generate candidate splitable set: bls---------------
	bls.reserve(Esize);
	bls.resize(0);
	size_t iE1 = 0, iE2 = 0;
	for (; iE1 < Esize; ++iE1) {
		StateID sE = E[iE1];
		BlockID blid = L[sE].blid;
		if (P[blid].blen != 1) { // remove State which blen==1 in E
			E[iE2++] = sE;
			bls.unchecked_push_back(blid);
		}
		assert(P[blid].blen > 0);
	}
	E.resize(Esize = iE2);
	bls.trim(std::unique(bls.begin(), bls.end()));
	if (bls.size() == 1) {
		BlockID blid = bls[0];
		if (E.size() == P[blid].blen) {
	//		printf("prefilter: E(s=%ld,c=%c) is not a splitter\n", (long)blid, E.ch);
			non_split_fast++;
			continue; // not a splitter
		}
		assert(E.size() < P[blid].blen);
	//	printf("single splitter\n");
	} else {
		std::sort(bls.begin(), bls.end());
		bls.trim(std::unique(bls.begin(), bls.end()));
	}
//	if (bls.size() > 1) printf("bls.size=%ld\n", (long)bls.size());
}//----End Generate candidate splitable set: bls---------------

#if defined(TRACE_AUTOMATA)
	if (bls.size() > 1)
	    printSplitter("  SPLITTER", E);
    size_t Bsum = 0; for (auto x : bls) Bsum += P[x].blen;
    if (bls.size() && Esize > 2*Bsum)
        printf("bigE=%ld, Bsum=%ld\n", (long)Esize, (long)Bsum);
#endif
    for (size_t i = 0; i < bls.size(); ++i) {
        assert(Esize > 0);
        const BlockID B_E = bls[i];   // (B -  E) take old BlockID of B
        const BlockID BxE = P.size(); // (B /\ E) take new BlockID
        const Block B = P[B_E]; // copy, not ref
        assert(B.blen > 1);
        printSplitter("  splitter", E);
//      printBlock(B_E);
        P.push_back();  // (B /\ E) will be in P.back() -- BxE
        size_t iE1 = 0, iE2 = 0;
        for (; iE1 < Esize; ++iE1) {
            StateID sE = E[iE1];
            if (L[sE].blid == B_E) {
                del(B_E, sE); // (B -  E) is in B_E
                add(BxE, sE); // (B /\ E) is in BxE
            }
		   	else E[iE2++] = sE; // E - B
        }
        E.resize(Esize = iE2); // let E = (E - B)
        assert(P[BxE].blen); // (B /\ E) is never empty
        assert(P[B_E].blen < B.blen); // assert P[B_E] not roll down
        if (P[B_E].blen == 0) { // (B - E) is empty
            P[B_E] = B; // restore the block
            fix_blid(B_E);
            P.pop_back(); // E is not a splitter for B
            non_split_slow++;
        } else {
			num_split++;
			if (P[B_E].isInW)
				putW(BxE);
			else
				putW(minBlock(B_E, BxE));
		}
    } // for
}
//-end split------------------------------------------------------------------
        ++iterations;
    }
#if defined(TRACE_AUTOMATA)
	printMappings();
	printf("refine completed, iterations=%ld, num_split=%ld, non_split[fast=%ld, slow=%ld], size[org=%ld, min=%ld]\n"
		  , iterations, num_split, non_split_fast, non_split_slow, (long)L.size(), (long)P.size());
#endif
        if (P[0].head != 0) { // initial state is not in block 0
            // put initial state into block 0, 0 is the initial state
            BlockID initial_block = L[0].blid;
            std::swap(P[0], P[initial_block]);
            fix_blid(0);
            fix_blid(initial_block);
        }
	} // refine1

	void fix_blid(size_t g) {
        const StateID B_head = P[g].head;
        StateID s = B_head;
        do {
            L[s].blid = g;
            s = L[s].next;
        } while (s != B_head);
    }
};

} // namespace terark

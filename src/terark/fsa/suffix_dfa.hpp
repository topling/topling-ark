#pragma once

#include "automata.hpp"

namespace terark {

template<class DFA, class LenType>
void
ComputeLongestPathLength1( const DFA& dfa
						 , typename DFA::state_id_t RootState
						 , valvec<LenType>& lens
						 )
{
	typedef typename DFA::state_id_t state_id_t;
	typedef typename DFA::transition_t transition_t;
	valvec<unsigned char> color(dfa.total_states(), 0);
	valvec<state_id_t> stack;
	valvec<state_id_t> index(dfa.total_states()+1, 0);
	valvec<state_id_t> backlink;
	state_id_t InverseRoot = DFA::nil_state;

	// for reduce memory usage, use indexed adjacent difference structure
	// first pass traverse dfa from RootState:
	//   caculate the number of incoming transitions of each state
	//   index[state_id] = number of incoming transitions of state_id
	stack.push_back(RootState);
	color[RootState] = 1;
	while (!stack.empty()) {
		state_id_t parent = stack.back(); stack.pop_back();
		dfa.for_each_dest(parent,
			[&](const transition_t& t) {
				state_id_t child = t;
				if (color[child] < 1) {
					color[child] = 1;
					stack.push_back(child);
				}
				index[child+1]++;
			});
		if (!dfa.has_children(parent)) {
			assert(DFA::nil_state == InverseRoot);
			if (DFA::nil_state != InverseRoot) {
				throw std::logic_error("ComputeLongestPathLength: more than one InverseRoot");
			}
			InverseRoot = parent;
		}
	}
	assert(DFA::nil_state != InverseRoot);
	if (DFA::nil_state == InverseRoot) {
		throw std::logic_error("ComputeLongestPathLength: not found InverseRoot");
	}
	if (!dfa.is_term(InverseRoot)) {
		throw std::logic_error("ComputeLongestPathLength: InverseRoot is not final state");
	}
	if (0 != index[RootState]) {
		throw std::logic_error("ComputeLongestPathLength: found back link to RootState");
	}
	for (size_t i = 2; i < index.size(); ++i) index[i] += index[i-1];

	// second pass traverse dfa from RootState:
	//   backlink[index[s] ... index[s+1]) is the backlinks of state s
	stack.push_back(RootState);
	color[RootState] = 2;
	backlink.resize_no_init(index.back());
	{
		valvec<state_id_t> index2 = index;
		while (!stack.empty()) {
			state_id_t parent = stack.back(); stack.pop_back();
			dfa.for_each_dest(parent,
				[&](const transition_t& t) {
					state_id_t child = t;
					if (color[child] < 2) {
						color[child] = 2;
						stack.push_back(child);
					}
					backlink[index2[child]++] = parent;
				});
		}
	}

	// traverse the inversed graph structure of dfa
	//   compute longest_path_length of state s from RootState
	lens.resize(dfa.total_states(), 0);
	stack.push_back(InverseRoot);
	color[InverseRoot] = 3;
	while (!stack.empty()) {
		state_id_t parent = stack.back();
		switch (color[parent]) {
		default:
			assert(0);
			throw std::runtime_error("ComputeLongestPathLength: unexpected 1");
		case 3: {
			size_t beg = index[parent+0];
			size_t end = index[parent+1];
			color[parent] = 4;
			for (size_t i = beg; i < end; ++i) {
				state_id_t child = backlink[i];
				switch (color[child]) {
				default:
					assert(0);
					throw std::runtime_error("ComputeLongestPathLength: unexpected 2");
					break;
				case 2: // not touched
					stack.push_back(child);
					color[child] = 3;
					break;
				case 3: // forward edge
					break;
				case 4: // back edge
					if (child == parent)
						throw std::logic_error("ComputeLongestPathLength: found a self-loop");
					else
						throw std::logic_error("ComputeLongestPathLength: found a non-self-loop");
				case 5: // cross edge
					break;
				}
			}
			break; }
		case 4: {
			size_t beg = index[parent+0];
			size_t end = index[parent+1];
			for (size_t i = beg; i < end; ++i) {
				state_id_t child = backlink[i];
				assert(5 == color[child]);
				lens[parent] = std::max(lens[parent], lens[child] + 1);
			}
			color[parent] = 5;
			stack.pop_back();
			break; }
		case 5:
			break;
		}
	}
}

template<class DFA, class LenType>
void
ComputeLongestPathLength2( const DFA& dfa
						 , typename DFA::state_id_t RootState
						 , valvec<LenType>& lens
						 )
{
	typedef typename DFA::state_id_t   state_id_t;
	typedef typename DFA::transition_t trans_t;
	valvec<unsigned char> color(dfa.total_states(), 2);
	valvec<state_id_t> stack, topological;
	lens.resize(dfa.total_states(), 0);
	stack.push_back(RootState);
	color[RootState] = 3;
	while (!stack.empty()) {
		state_id_t parent = stack.back();
		switch (color[parent]) {
		default:
			assert(0);
			throw std::runtime_error("ComputeLongestPathLength: unexpected 1");
		case 3:
			color[parent] = 4;
			dfa.for_each_dest(parent, [&](const trans_t& t) {
				state_id_t child = t;
				switch (color[child]) {
				default:
					assert(0);
					throw std::runtime_error("ComputeLongestPathLength: unexpected 2");
					break;
				case 2: // not touched
					stack.push_back(child);
					color[child] = 3;
					break;
				case 3: // forward edge
					break;
				case 4: // back edge
					if (child == parent)
						throw std::logic_error("ComputeLongestPathLength: found a self-loop");
					else
						throw std::logic_error("ComputeLongestPathLength: found a non-self-loop");
				case 5: // cross edge
					break;
				}
			});
			break;
		case 4:
			topological.push_back(parent);
			color[parent] = 5;
			stack.pop_back();
			break;
		case 5:
			break;
		}
	}
	for (size_t i = topological.size(); i > 0; --i) {
		state_id_t parent = topological[i-1];
		dfa.for_each_dest(parent, [&,parent](const trans_t& t) {
			state_id_t child = t;
			assert(5 == color[child]);
			lens[child] = std::max(lens[parent] + 1, lens[child]);
		});
	}
}

#define ComputeLongestPathLength ComputeLongestPathLength2

template<class DFA>
void CreateSuffixDFA(DFA& A, typename DFA::state_id_t Root = initial_state) {
	typedef typename DFA::state_id_t state_id_t;
	typedef typename DFA::transition_t  trans_t;
	valvec<state_id_t> F, L, Q1, Q2, S(A.total_states(), Root);
	state_id_t MostFinal = A.nil_state;
	S[Root] = A.nil_state;
	auto SuffixNext = [&](state_id_t p, state_id_t q, auchar_t c) {
		state_id_t t = A.nil_state;
		while (p != Root) {
			t = A.state_move(p, c);
			if (A.nil_state == t) {
				A.add_move(p, q, c);
				p = S[p];
			} else
				goto Next;
		}
		assert(A.nil_state == t);
		A.add_move(Root, q, c);
		S[q] = Root;
		return;
	Next:
		if (L[p] + 1 == L[t] && t != q) {
			S[q] = t;
		} else {
			state_id_t r;
			if (t == q) {
				r = t;
			} else {
				r = A.clone_state(t);
				L.resize(A.total_states(), 0);
				S.resize(A.total_states(), Root);
				S[q] = r;
			}
			S[r] = S[t];
			S[t] = S[r];
			L[r] = L[p] + 1;
			assert(A.state_move(p, c) != A.nil_state);
			while (A.nil_state != p &&
				   L[A.state_move(p, c)] >= L[r])
		   	{
				A.set_move(p, r, c);
				p = S[p];
			}
		}
	};
	auto SuffixFinal = [&](state_id_t p) {
		while (A.nil_state != (p = S[p]) && !A.is_term(p))
			A.set_term_bit(p);
	};
	ComputeLongestPathLength(A, Root, L);
	for (size_t i = 0; i < L.size(); ++i) {
		printf("L[%04zd] = %u\n", i, L[i]);
	}
	valvec<CharTarget<state_id_t> > MostFinal_src;

	A.resize_states(A.total_states());
	Q1.push_back(Root);
	febitvec mark(A.total_states());
//	valvec<CharTarget<state_id_t> > children;
	L.resize(A.total_states());
	while (!Q1.empty()) {
		for (state_id_t p : Q1) {
//			children.erase_all();
			A.for_each_move(p,[&](const trans_t& t,auchar_t c){
				state_id_t q = t;
				bool q_has_children = A.has_children(q);
				if (mark.is0(q)) {
					if (!q_has_children) {
						assert(A.nil_state == MostFinal);
						assert(A.is_term(q));
						MostFinal_src.emplace_back(c, p);
						MostFinal = q;
					}
					if (A.is_term(q))
						F.push_back(q);
					mark.set1(q);
					Q2.push_back(q);
				}
				if (q_has_children)
					SuffixNext(p, q, c);
			});
		}
		Q1.swap(Q2);
		Q2.erase_all();
	}
	assert(A.nil_state != MostFinal);
	for (auto ct : MostFinal_src) {
		state_id_t src = ct.target;
		SuffixNext(src, MostFinal, ct.ch);
		SuffixFinal(MostFinal);
	}
	for (state_id_t p : F)
	   	SuffixFinal(p);
}

} // namespace terark

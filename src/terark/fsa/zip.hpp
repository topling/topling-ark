#pragma once

#include "automata.hpp"

template<class TrieDFA>
class ProductionTrie {
	BOOST_STATIC_ASSERT(TrieDFA::sigma > 256);

public:
	typedef  typename TrieDFA::state_id_t id_t;

private:
	TrieDFA  m_trie;
	size_t   m_max_id;

public:
	std::pair<size_t, bool>
	add_seq(const id_t* seq, size_t len) {
		fstring str((const char*)seq, sizeof(id_t) * len);
		std::pair<size_t, bool> tb = m_trie.add_word_aux(initial_state, str);
		size_t tail = tb.first;
		if (tb.second) {
			m_trie.add_move_checked(tail, m_max_id, 256);
			return std::make_pair(m_max_id++, true);
		} else {
			id_t id = m_trie.state_move(tail, 256);
			return std::make_pair(id, false);
		}
	}

	template<class InputRange>
	bool
	max_match(InputRange& ir, size_t* pval, size_t* plen) {
		size_t curr = initial_state;
		size_t last_match_val = size_t(-1);
		size_t last_match_len = 0;
		size_t curr_match_len = 0;
		while (!ir.empty()) {
			union {
				id_t     id;
				uint08_t u8[sizeof(id_t)];
			} bx;
			bx.id = *ir;
			for(size_t j = 0; j < sizeof(id_t); ++j) {
				size_t next = m_trie.state_move(curr, bx[j]);
				if (m_trie.nil_state == next) {
					curr_match_len++;
					goto Done;
				}
				curr = next;
			}
			size_t val = m_trie.state_move(curr, 256);
			if (m_trie.nil_state != val) {
				last_match_len = curr_match_len;
				last_match_val = val;
				//THROW_STD(logic_error, "Unexpected");
			}
			++ir;
			++curr_match_len;
		}
	Done:
		if (size_t(-1) != last_match_val) {
			*plen = last_match_len;
			*pval = last_match_val;
			return true;
		}
		return false;
	}
};

struct DFA_Zip_StateHashFunc {
	mutable valvec<size_t> m_stack1;
	mutable valvec<size_t> m_stack2;
	StateToTreeAuSigmaMap* m_ssmap;
	const DFA* m_dfa;
	size_t m_max_key_len;

	size_t hash(size_t root) const;
	bool equal(size_t root1, size_t root2) const;
};

template<class DFA, class StateToTreeAuSigmaMap>
class DFA_Zip_StateHashTab : public
	gold_hash_tab
		<
		, typename DFA::state_id_t
		, typename DFA::state_id_t
		, HashEqual
		>
{
public:
	typedef typename state_id_t dfa_id_t;
};

template<class DFA, class StateToTreeAuSigmaMap>
size_t DFA_Zip_StateHashFunc::hash_code(size_t root) {
	// generate TreeDFS sequence of DFA start at root
	// compute hash for m_ssmap[sequence]
	assert(m_stack1.empty());
	m_stack1.push(root);
	size_t h = 0;
	size_t i = 0;
	while (i < m_max_key_len && !m_stack1.empty()) {
		size_t parent = m_stack1.back(); m_stack1.pop_back();
		dfa_id_t ts_id = m_ssmap->find_i(parent); // Tree Au Sigma ID
		assert(ts_id < m_ssmap->end_i());
		h = HashCombine(h, ts_id);
		size_t oldsize = m_stack1.size();
		m_stack1.resize_no_init(oldsize + 512);
		size_t cnt = m_dfa->get_all_dest(parent, &m_stack1[oldsize]);
		m_stack1.risk_set_size(oldsize + cnt);
		std::reverse(m_stack1.begin()+oldsize, m_stack1.end());
		i++;
	}
	return h;
}

template<class DFA, class StateToTreeAuSigmaMap>
bool DFA_Zip_StateHashFunc::equal(size_t root1, size_t root2) {
	assert(m_stack1.empty());
	assert(m_stack2.empty());
	m_stack1.push(root1);
	m_stack2.push(root2);
	size_t i = 0;
	while (i < m_max_key_len && !m_stack1.empty() && !m_stack2.empty()) {
		size_t parent1 = m_stack1.back(); m_stack1.pop_back();
		size_t parent2 = m_stack2.back(); m_stack2.pop_back();
		dfa_id_t ts_id1 = m_ssmap->find_i(parent1);
		dfa_id_t ts_id2 = m_ssmap->find_i(parent2);
		if (ts_id1 != ts_id2)
			return false;
		assert(ts_id1 < m_ssmap->end_i());
		assert(ts_id2 < m_ssmap->end_i());
		size_t oldsize1 = m_stack1.size();
		size_t oldsize2 = m_stack2.size();
		m_stack1.resize_no_init(oldsize1 + 512);
		m_stack2.resize_no_init(oldsize2 + 512);
		size_t cnt1 = m_dfa->get_all_dest(parent1, &m_stack1[oldsize]);
		size_t cnt2 = m_dfa->get_all_dest(parent2, &m_stack2[oldsize]);
		if (cnt1 != cnt2) {
			m_stack1.risk_set_size(0);
			m_stack2.risk_set_size(0);
			return false;
		}
		m_stack1.risk_set_size(oldsize + cnt1);
		m_stack2.risk_set_size(oldsize + cnt2);
		std::reverse(m_stack1.begin()+oldsize, m_stack1.end());
		std::reverse(m_stack2.begin()+oldsize, m_stack2.end());
		i++;
	}
	return true;
}

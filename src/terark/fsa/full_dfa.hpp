#pragma once

#include "dfa_algo.hpp"
#include <terark/gold_hash_idx.hpp>
#include <terark/gold_hash_map.hpp>

namespace terark {

template<class StateID>
class FullDFA : public BaseDFA {
protected:
	const static StateID TermBit = StateID(1) << (sizeof(StateID) * 8 - 1);
	static StateID get_map_id(StateID val) { return val & ~TermBit; }

	struct IndexMapEntry {
		uint08_t idxmap[256];
		uint32_t refcnt = 0;
		uint16_t n_uniq = 0;
		uint16_t n_char = 0;
		IndexMapEntry() {
			std::fill_n(idxmap, 256, 255);
		}
	};

public:
	typedef StateID transition_t;
	typedef StateID state_id_t;
	typedef StateID state_t, State;

	static const state_id_t max_state = StateID(-1) - 1;
	static const state_id_t nil_state = StateID(-1);
	enum { sigma = 256 };

	size_t total_states() const { return states.size(); }
	size_t num_zpath_states() const { return 0; }
	size_t total_transitions() const { return 256 * this->total_states(); }
	size_t mem_size() const override final {
		return states.used_mem_size() + m_index_map.used_mem_size();
	}
	size_t v_gnode_states() const override final { return m_gnode_states; }

	fstring get_zpath_data(size_t, MatchContext*) const {
		THROW_STD(invalid_argument, "Invalid call");
	}
	template<class... Args>
	void copy_from(const Args&... args) {
		THROW_STD(invalid_argument, "This is a stub function, should not be called");
	}

	~FullDFA() {
		if (this->mmap_base) {
			states.risk_release_ownership();
			m_roots.risk_release_ownership();
			m_index_map.risk_release_ownership();
		}
	}

	void clear() {
		states.clear();
		m_roots.clear();
		m_index_map.clear();
	}

	void risk_swap(FullDFA& y) {
		BaseDFA::risk_swap(y);
		states.swap(y.states);
		m_roots.swap(y.m_roots);
		m_index_map.swap(y.m_index_map);
		std::swap(m_atom_dfa_num, y.m_atom_dfa_num);
		std::swap(m_gnode_states, y.m_gnode_states);
		std::swap(m_dfa_cluster_num, y.m_dfa_cluster_num);
		std::swap(m_transition_num, y.m_transition_num);
	}
	void swap(FullDFA& y) {
		assert(typeid(*this) == typeid(y));
		risk_swap(y);
	}

	size_t v_num_children(size_t) const override { TERARK_DIE("should not call"); }
	bool v_has_children(size_t s) const override final { return true; }

	bool has_freelist() const override { return false; }
	bool is_free(size_t state) const { return false; }
	bool is_pzip(size_t state) const { return false; }
	bool is_term(size_t state) const { return (states[state] & TermBit) != 0; }

	size_t state_move(size_t state, auchar_t ch) const {
		assert(ch < 256);
		assert(state < states.size());
		size_t map_id = get_map_id(states[state]);
		assert(map_id < m_index_map.size());
		size_t ch_id = m_index_map[map_id].idxmap[ch];
		return states[state + 1 + ch_id];
	}
	template<class OP>
	void for_each_move(state_id_t state, OP op) const {
		assert(state < states.size());
		size_t map_id = get_map_id(states[state]);
		assert(map_id < m_index_map.size());
		const auto& ent = m_index_map[map_id];
		assert(state + ent.n_uniq < states.size());
		for (auchar_t ch = 0; ch < 256; ++ch) {
			size_t child = states[state + 1 + ent.idxmap[ch]];
			if (nil_state != child)
				op(child, ch);
		}
	}
	template<class OP>
	void for_each_dest(state_id_t state, OP op) const {
		assert(state < states.size());
		size_t map_id = get_map_id(states[state]);
		assert(map_id < m_index_map.size());
		const auto& ent = m_index_map[map_id];
		assert(state + ent.n_uniq < states.size());
		for (auchar_t ch = 0; ch < 256; ++ch) {
			size_t child = states[state + 1 + ent.idxmap[ch]];
			if (nil_state != child)
				op(child);
		}
	}
	template<class OP>
	void for_each_dest_rev(state_id_t state, OP op) const {
		assert(state < states.size());
		size_t map_id = get_map_id(states[state]);
		assert(map_id < m_index_map.size());
		const auto& ent = m_index_map[map_id];
		assert(state + ent.n_uniq < states.size());
		for (auchar_t ch = 256; ch > 0;) {
			size_t child = states[state + 1 + ent.idxmap[--ch]];
			if (nil_state != child)
				op(child);
		}
	}

	struct HashEntry {
		StateID refcnt = 0;
		StateID n_uniq;
		StateID n_char;
		StateID link = gold_hash_idx_default_bucket<StateID>::delmark;
	};
	struct LinkStore : valvec<HashEntry>, gold_hash_idx_default_bucket<StateID> {
		typedef StateID link_t;
		link_t  link(size_t idx) const { return (*this)[idx].link; }
		void setlink(size_t idx, link_t val) { (*this)[idx].link = val; }
	};
	struct HashEqual {
		valvec<IndexMapEntry>* m_homo_keys;

		size_t hash(size_t key) const {
			const size_t* px = (const size_t*)((*m_homo_keys)[key].idxmap);
			size_t hval = 342;
			for (size_t i = 0; i < 256 / sizeof(size_t); ++i) {
				hval = FaboHashCombine(hval, px[i]);
			}
			return hval;
		}
		bool equal(size_t x, size_t y) const {
			const size_t* px = (const size_t*)((*m_homo_keys)[x].idxmap);
			const size_t* py = (const size_t*)((*m_homo_keys)[y].idxmap);
			for (size_t i = 0; i < 256 / sizeof(size_t); ++i)
				if (px[i] != py[i])
					return false;
			return true;
		}
	};
	struct MyHashTable : gold_hash_idx1<LinkStore, HashEqual> {
		using HashEqual::m_homo_keys;
		explicit MyHashTable(valvec<IndexMapEntry>* homo_keys) {
			this->m_homo_keys = homo_keys;
		}
		gold_hash_tab<size_t> m_serial_key;
		size_t find_or_add(const CharTarget<size_t>* moves, size_t n_char) {
			assert(n_char <= 256);
			m_homo_keys->push_back();
			this->get_link_store().push_back();
			auto& ent = m_homo_keys->back();
			m_serial_key.erase_all();
			for(size_t i = 0; i < n_char; ++i) {
				const auto& ct = moves[i];
				assert(ct.ch < 256);
				size_t id = m_serial_key.insert_i(ct.target).first;
				ent.idxmap[ct.ch] = byte_t(id);
			}
			if (n_char < 256) { // has nil_state move
				size_t id = m_serial_key.insert_i(size_t(-1)).first;
				std::replace(ent.idxmap, ent.idxmap+256, byte_t(255), byte_t(id));
			}
			size_t idx = m_homo_keys->size() - 1;
			size_t found = this->insert_at(idx);
			assert(found <= idx);
			if (found == idx) {
				(*m_homo_keys)[found].n_char = (uint16_t)n_char;
				(*m_homo_keys)[found].n_uniq = (uint16_t)m_serial_key.end_i();
			} else {
				assert(!m_homo_keys->empty());
				m_homo_keys->pop_back();
			}
			return found;
		}
	};

	template<class Au, class AuID>
	void build_from(const Au& au, const AuID* src_roots, size_t num_roots) {
		assert(au.get_sigma() >= 256);
		m_roots.resize_fill(num_roots, StateID(nil_state));
		MyHashTable homo(&m_index_map); // homomorphism states
		febitvec color(au.total_states(), valvec_no_init());
		valvec<AuID>  stack;
		AutoFree<state_id_t> state_map(au.total_states(), state_id_t(-1));
		valvec<int> match_id_vec;
		AutoFree<CharTarget<size_t> > moves(au.get_sigma());
		size_t n_move, n_char;
		size_t dst_id = 0;
		m_gnode_states = 0;
		m_transition_num = 0;

		auto prepare_walk = [&]() {
			dst_id = 0;
			color.fill(false);
		};

		auto put_children = [&](size_t parent) {
			for (size_t i = n_char; i;) {
				const auto& ct = moves.p[--i];
				if (color.is0(ct.target)) {
					color.set1(ct.target);
					stack.push_back(ct.target);
				}
			}
		};

		prepare_walk(); // first pass
		for(size_t i = num_roots; i;) {
			size_t root = src_roots[--i];
			if (color.is0(root)) {
				color.set1(root);
				stack.push(root);
			}
		}
		while (!stack.empty()) {
			size_t parent = stack.pop_val();
			n_move = au.get_all_move(parent, moves.p);
			n_char = (au.sigma > 256)
				? lower_bound_ex_0(moves.p, n_move, 256, CharTarget_By_ch())
				: n_move;
			size_t map_id = homo.find_or_add(moves.p, n_char);
			size_t n_uniq = this->m_index_map[map_id].n_uniq;
			this->m_index_map[map_id].refcnt++;
			state_map.p[parent] = dst_id;
			dst_id += 1 + n_uniq;
			if (au.is_term(parent)) {
				match_id_vec.erase_all();
				dfa_read_matchid(&au, parent, &match_id_vec);
				dst_id += 1 + match_id_vec.size();
			}
			m_gnode_states++;
			m_transition_num += n_char;
			put_children(parent);
		}

		this->states.resize_no_init(dst_id);

		prepare_walk(); // second pass
		for(size_t i = num_roots; i;) {
			size_t root = src_roots[--i];
			if (color.is0(root)) {
				color.set1(root);
				stack.push(root);
			}
			m_roots[i] = state_map.p[root];
		}
		while (!stack.empty()) {
			size_t parent = stack.pop_val();
			n_move = au.get_all_move(parent, moves.p);
			n_char = (au.sigma > 256)
				? lower_bound_ex_0(moves.p, n_move, 256, CharTarget_By_ch())
				: n_move;
			size_t map_id = homo.find_or_add(moves.p, n_char);
			size_t n_uniq = m_index_map[map_id].n_uniq;
			assert(state_map.p[parent] == dst_id);
			size_t next = dst_id;
			this->states[next++] = map_id;
			assert(homo.m_serial_key.end_i() == n_uniq);
			for(size_t i = 0; i < n_uniq; ++i) {
				size_t target = homo.m_serial_key.key(i);
				this->states[next++] = size_t(-1) == target
					? nil_state
					: state_map.p[target];
			}
			if (au.is_term(parent)) {
				this->states[dst_id] |= TermBit;
				match_id_vec.erase_all();
				dfa_read_matchid(&au, parent, &match_id_vec);
				this->states[next++] = match_id_vec.size();
				std::copy_n(match_id_vec.begin(),
					match_id_vec.size(),
					this->states.data() + next);
				next += match_id_vec.size();
			}
			dst_id = next;
			put_children(parent);
		}
#if !defined(NDEBUG)
		for (size_t i = 0; i < au.total_states(); ++i) {
			if (!au.is_free(i))
				check_state(au, i, state_map.p);
		}
#endif
	}

	template<class Au>
	void check_state(const Au& au, size_t src_state, const StateID* state_map) const {
#if !defined(NDEBUG)
		AutoFree<CharTarget<size_t> > moves(au.get_sigma());
		size_t n_move = au.get_all_move(src_state, moves.p);
		size_t n_char = lower_bound_ex_0(moves.p, n_move, 256, CharTarget_By_ch());
		size_t dst_state = state_map[src_state];
		assert(this->is_term(dst_state) == au.is_term(src_state));
		for (size_t i = 0; i < n_char; ++i) {
			const auto& ct = moves.p[i];
			size_t dst_target1 = state_map[ct.target];
			size_t dst_target2 = this->state_move(dst_state, ct.ch);
			assert(dst_target1 == dst_target2);
		}
		n_char = this->get_all_move(dst_state, moves.p);
		for(size_t i = 0; i < n_char; ++i) {
			const auto& ct = moves.p[i];
			size_t dst_target1 = ct.target;
			size_t dst_target2 = this->state_move(dst_state, ct.ch);
			assert(dst_target1 == dst_target2);
		}
#endif
	}

	friend size_t dfa_matchid_root(const FullDFA* dfa, size_t state) {
		return dfa->is_term(state) ? state : dfa->nil_state;
	}
	friend size_t dfa_read_matchid(const FullDFA* dfa, size_t state, valvec<int>* vec) {
		assert(state < dfa->states.size());
		assert(dfa->is_term(state));
		size_t map_id = get_map_id(dfa->states[state]);
		assert(map_id < dfa->m_index_map.size());
		size_t n_uniq = dfa->m_index_map[map_id].n_uniq;
		size_t n_match = dfa->states[state + 1 + n_uniq];
		assert(state + 2 + n_uniq + n_match <= dfa->states.size());
		vec->append(dfa->states.data() + state + 2 + n_uniq, n_match);
		return n_match;
	}

	size_t m_atom_dfa_num = 0;
	size_t m_dfa_cluster_num = 0;

protected:
	valvec<state_t> states;
	valvec<state_id_t> m_roots;
	size_t m_gnode_states;
	size_t m_transition_num;
	valvec<IndexMapEntry> m_index_map;

	typedef FullDFA MyType;
//#include "ppi/for_each_suffix.hpp"
//#include "ppi/match_key.hpp"
//#include "ppi/match_path.hpp"
//#include "ppi/match_prefix.hpp"
#include "ppi/dfa_const_virtuals.hpp"
#include "ppi/state_move_fast.hpp"
//#include "ppi/for_each_word.hpp"

	template<class DataIO>
	friend void DataIO_loadObject(DataIO&, FullDFA&) {
		THROW_STD(invalid_argument, "Unsupported");
	}
	template<class DataIO>
	friend void DataIO_saveObject(DataIO&, const FullDFA&) {
		THROW_STD(invalid_argument, "Unsupported");
	}

	void finish_load_mmap(const DFA_MmapHeader* base) override {
		assert(sizeof(State) == base->state_size);
		byte_t* bbase = (byte_t*)base;
		if (base->total_states >= max_state) {
			THROW_STD(out_of_range, "total_states=%lld", (long long)base->total_states);
		}
		states.clear();
		states.risk_set_data((State*)(bbase + base->blocks[0].offset));
		states.risk_set_size(size_t(base->total_states));
		states.risk_set_capacity(size_t(base->total_states));
		assert(base->blocks[1].length > 0);
		assert(base->blocks[1].length % sizeof(state_id_t) == 0);
		size_t root_num = base->blocks[1].length / sizeof(state_id_t);
		m_roots.risk_set_data((state_id_t*)(bbase + base->blocks[1].offset));
		m_roots.risk_set_size(root_num);
		m_roots.risk_set_capacity(root_num);
		size_t index_map_size = base->blocks[2].length / sizeof(IndexMapEntry);
		m_index_map.risk_set_data((IndexMapEntry*)(bbase + base->blocks[2].offset));
		m_index_map.risk_set_size(index_map_size);
		m_index_map.risk_set_capacity(index_map_size);
		m_gnode_states = size_t(base->gnode_states);
		m_zpath_states = size_t(base->zpath_states);
		m_atom_dfa_num = base->atom_dfa_num;
		m_dfa_cluster_num = base->dfa_cluster_num;
		m_total_zpath_len = base->zpath_length;
		m_transition_num = base->transition_num;
		assert(m_dfa_cluster_num + m_atom_dfa_num == root_num);
	}

	long prepare_save_mmap(DFA_MmapHeader* base, const void** dataPtrs)
	const override {
		base->state_size = sizeof(State);
		base->num_blocks = 3;
		base->atom_dfa_num = m_atom_dfa_num;
		base->dfa_cluster_num = m_roots.size() - m_atom_dfa_num;
		base->zpath_length = m_total_zpath_len;
		base->zpath_states = m_zpath_states;
		base->transition_num = m_transition_num;

		base->blocks[0].offset = sizeof(DFA_MmapHeader);
		base->blocks[0].length = sizeof(State)*states.size();
		base->blocks[1].offset = align_to_64(base->blocks[0].endpos());
		base->blocks[1].length = m_roots.size() * sizeof(state_id_t);
		base->blocks[2].offset = align_to_64(base->blocks[1].endpos());
		base->blocks[2].length = m_index_map.size() * sizeof(IndexMapEntry);
		dataPtrs[0] = states.data();
		dataPtrs[1] = m_roots.data();
		dataPtrs[2] = m_index_map.data();

		return 0;
	}

	void print_index_map(FILE* fp) const {
		for (size_t map_id = 0; map_id < m_index_map.size(); ++map_id) {
			fprintf(fp
				, "map_index[%02zd]: chars = %3u uniq = %2u, refcnt = %u\n"
				, map_id
				, m_index_map[map_id].n_char
				, m_index_map[map_id].n_uniq
				, m_index_map[map_id].refcnt
				);
		}
		fprintf(fp, "\n");
	}
};

} // namespace terark

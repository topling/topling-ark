#pragma once

#include <string>
#include <limits.h>
#include "dfa_algo.hpp"

namespace terark {

class StringAsDFA : public BaseDFA {
public:
	std::string str;

	StringAsDFA() { m_dyn_sigma = sigma; }
	explicit StringAsDFA(const std::string& s) : str(s) {}

	typedef unsigned state_id_t;
	typedef unsigned transition_t;
	static const state_id_t nil_state = UINT_MAX;
	static const state_id_t max_state = UINT_MAX - 1;
	enum { sigma = 256 };

	size_t total_states() const { return str.size() + 1; }

	bool is_term(state_id_t s) const {
		assert(s <= str.size());
	   	return s == str.size();
	}
	bool is_free(state_id_t s) const { return false; }

	bool has_freelist() const override final { return false; }
	size_t mem_size() const override final { return str.size()+1; }
	size_t v_gnode_states() const override final { return str.size()+1; }

	template<class OP>
	void for_each_move(state_id_t curr, OP op) const {
		if (str.size() != curr)
			op(curr+1, (unsigned char)str[curr]);
	}
	template<class OP>
	void for_each_dest(state_id_t curr, OP op) const {
		if (str.size() != curr)
			op(curr+1);
	}
	template<class OP> // same with for_each_dest
	void for_each_dest_rev(state_id_t curr, OP op) const {
		if (str.size() != curr)
			op(curr+1);
	}
	state_id_t add_move(state_id_t source, state_id_t target, unsigned char ch) {
		assert(source <= str.size());
		assert(target <= str.size());
		return target;
	}
#if 0
	state_id_t set_move(state_id_t source, state_id_t target, unsigned char ch) {
		assert(source <= str.size());
		assert(target <= str.size());
		assert(0);
		throw std::logic_error("calling StringAsDFA::set_move");
	}
#endif
	state_id_t state_move(state_id_t curr, unsigned char ch) const {
		assert(curr <= str.size());
		if (curr < str.size() && ch == (unsigned char)str[curr])
			return curr + 1;
		else
			return nil_state;
	}
	size_t v_num_children(size_t s) const override final {
		return s < str.size() ? 1 : 0;
	}
	bool v_has_children(size_t s) const override final {
		assert(s <= str.size());
		return s != str.size();
	}
	bool has_children(state_id_t s) const {
        assert(s <= str.size());
		return s != str.size();
	}

	bool is_pzip(size_t i) const {
		assert(i <= str.size());
		return false;
	}
	fstring get_zpath_data(size_t, MatchContext*) const {
		assert(0);
		abort();
		return "";
	}

	void shrink_to_fit() {}

	struct dfs_walker {
		state_id_t curr;
		state_id_t nstr;
		void resize(size_t NumStates) {	nstr = NumStates; }
		void putRoot(state_id_t RootState) { curr = RootState; }
		bool is_finished() const {
			return nstr < curr;
		}
		state_id_t next() {
			assert(curr <= nstr);
		   	return curr;
	   	}
		void putChildren(const StringAsDFA* au, state_id_t parent) {
			assert(au->str.size() >= curr);
			assert(au->str.size() == nstr);
		   	curr = parent + 1;
	   	}
	};
	typedef dfs_walker bfs_walker, pfs_walker;

	size_t num_free_states() const { return 0; }
	size_t num_used_states() const { return str.size() + 1; }

	friend size_t
	GraphTotalVertexes(const StringAsDFA* dfa) { return dfa->total_states(); }

	void print_output(FILE* fp) const {
		fprintf(fp, "%s\n", str.c_str());
	}
	void print_output(const char* fname) const {
		Auto_fclose fp(fopen(fname, "w"));
		if (fp) {
			fprintf(fp, "%s\n", str.c_str());
		} else {
			fprintf(stderr, "Fail: fopen(%s, w) = %s\n", fname, strerror(errno));
		}
	}

typedef StringAsDFA MyType;
#include "ppi/dfa_const_virtuals.hpp"
#include "ppi/state_move_fast.hpp"
};

} // namespace terark

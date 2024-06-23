protected:
enum class Color : char {
	White,
	Grey,
	Black,
};
bool has_cycle_loop(state_id_t curr, Color* mark) const {
	if (Color::Grey == mark[curr])
		return true;
	if (Color::Black == mark[curr])
		return false;
	mark[curr] = Color::Grey;
	bool hasCycle = false;
	for_each_dest(curr, [=,&hasCycle](state_id_t t) {
		hasCycle = hasCycle || this->has_cycle_loop(t, mark);
	});
	mark[curr] = Color::Black;
	return hasCycle;
}

/// @return paths_from_state[curr], MY_SIZE_T_MAX indicate path num overflow
size_t dag_pathes_loop(state_id_t curr, Color* mark
				   , size_t* paths_from_state) const {
//	printf("curr=%ld\n", (long)curr);
	const size_t MY_SIZE_T_MAX = size_t(-1);
	assert(curr < (state_id_t)total_states());
	if (Color::Grey == mark[curr]) {
		paths_from_state[curr] = MY_SIZE_T_MAX;
		paths_from_state[initial_state] = MY_SIZE_T_MAX;
		assert(0);
		throw std::logic_error("DFA_Algorithm::dag_pathes: encountered a loop");
	}
	if (Color::Black == mark[curr]) {
		assert(paths_from_state[curr] != 0);
		return paths_from_state[curr];
	}
	mark[curr] = Color::Grey;
	size_t num = 0;
	try {
		for_each_dest(curr, [&](state_id_t dest) {
			size_t subs = dag_pathes_loop(dest, mark, paths_from_state);
			if (num + subs < num) { // overflow
				num = MY_SIZE_T_MAX;
				throw int(0);
			}
			num += subs;
		});
	} catch (int) {
		goto Done;
	}
	if (num < MY_SIZE_T_MAX) {
		if (is_term(curr))
			num++;
	}
Done:
	assert(num >= 1);
//	assert(num < MY_SIZE_T_MAX);
//	printf("**curr=%ld, num=%ld\n", (long)curr, (long)num);
	paths_from_state[curr] = num;
	mark[curr] = Color::Black;
	return num;
}

public:
bool tpl_is_dag() const {
	valvec<Color> mark(total_states(), Color::White);
	return !has_cycle_loop(initial_state, &mark[0]);
}
// return size_t(-1) indicate there is some loop in the state graph
size_t dag_pathes0() const {
	assert(num_used_states() >= 1);
	valvec<Color> mark(total_states(), Color::White);
	valvec<size_t> paths_from_state(total_states());
	return dag_pathes_loop(initial_state, &mark[0], &paths_from_state[0]);
}
size_t dag_pathes() const {
	valvec<size_t> pathes;
	return dag_pathes(pathes);
}
size_t dag_pathes(valvec<size_t>& pathes) const {
	assert(num_used_states() >= 1);
	pathes.resize_no_init(total_states());
#ifndef NDEBUG
	pathes.fill(size_t(-2));
#endif
	topological_sort([&](state_id_t parent) {
		assert(size_t(-2) == pathes[parent]);
		size_t np = this->is_term(parent) ? 1 : 0;
		this->for_each_dest(parent,
			[&](state_id_t child) {
				assert(size_t(-2) != pathes[child]);
				size_t nc = pathes[child];
				if (size_t(-1) != np) {
					if (size_t(-1) == nc || np + nc < np)
						np = size_t(-1);
					else
						np += pathes[child];
				}
			});
		pathes[parent] = np;
	});
	return pathes[0];
}

struct LenNum {
	size_t len;
	size_t num;
	LenNum() : len(0), num(0) {}
};

// @param num_words is identical to the number of total pathes
size_t dag_total_path_len(size_t* num_words = NULL) const {
	assert(num_used_states() >= 1);
	valvec<state_id_t> rev_topo(total_states(), valvec_reserve());
	topological_sort(initial_state,
		[&](state_id_t s){rev_topo.unchecked_push_back(s);});
	assert(rev_topo.size() <= total_states());
	if (num_words)
	   *num_words = 0;
	try {
		valvec<LenNum> path(total_states());
		MatchContext   ctx;
		size_t total_len = 0;
		size_t total_path = 0;
		path[initial_state].num = 1;
		for(size_t i = rev_topo.size(); i > 0; --i) {
			size_t parent = rev_topo[i-1];
			size_t step_len = 1;
			if (is_pzip(parent)) {
				step_len += get_zpath_data(parent, &ctx).size();
			}
			size_t np = path[parent].num;
			size_t lp = path[parent].len + step_len * np;
			for_each_dest(parent,
				[&](size_t child) {
					LenNum& c = path[child];
					if (lp + c.len < std::max(lp, c.len))
						throw std::out_of_range("path length too large");
					else
						c.len += lp, c.num += np;
				});
			if (is_term(parent)) {
				size_t lp0 = path[parent].len;
				if (total_len + lp0 < std::max(total_len, lp0))
					return -1;
				else
					total_len += lp0, total_path += np;
			}
		}
		if (num_words)
		   *num_words = total_path;
		return total_len;
	}
	catch (const std::out_of_range&) {
		return -1;
	}
}

template<class OnFinish>
void topological_sort(OnFinish on_finish) const {
	topological_sort2<OnFinish>(initial_state, on_finish);
}
template<class OnFinish>
void topological_sort(state_id_t RootState, OnFinish on_finish) const {
	topological_sort2<OnFinish>(RootState, on_finish);
}
template<class OnFinish>
void topological_sort(valvec<state_id_t>& stack, OnFinish on_finish) const {
	topological_sort2<OnFinish>(stack, on_finish);
}

template<class OnFinish, class OnBackEdge>
void
dfs_post_order_walk(state_id_t RootState, OnFinish on_finish, OnBackEdge on_back_edge)
const {
	// non-recursive topological sort
	assert(RootState < total_states());
	valvec<state_id_t> stack;
	stack.reserve(1024);
	stack.push_back(RootState);
	dfs_post_order_walk<OnFinish, OnBackEdge>(stack, on_finish, on_back_edge);
}
template<class OnFinish, class OnBackEdge>
void // allowing stack to have multiple roots
dfs_post_order_walk(valvec<state_id_t>& stack, OnFinish on_finish, OnBackEdge on_back_edge)
const {
	assert(!stack.empty());
	valvec<Color> mark(total_states(), Color::White);
	while (!stack.empty()) {
		state_id_t curr = stack.back();
		switch (mark[curr]) {
		default:
			assert(0);
			break;
		case Color::White: // not expanded yet
			mark[curr] = Color::Grey;
			for_each_dest_rev(curr,
			[&](state_id_t child) {
				switch (mark[child]) {
				default:
					assert(0);
					break;
				case Color::White: // tree edge
					// child may have multiple copies in stack
					// but this doesn't matter, just 1 will be expanded
					stack.push_back(child);
					break;
				case Color::Grey: // back edge
					on_back_edge(curr, child);
					break;
				case Color::Black: // cross edge
					break;
				}
			});
			break;
		case Color::Grey:
			mark[curr] = Color::Black;
			on_finish(curr);
			// fall through:
			no_break_fallthrough;
		case Color::Black: // forward edge
			stack.pop_back();
			break;
		}
	}
}

// used for multiple-pass topological sort start from different root
template<class ColorID, class OnFinish>
void topological_sort( valvec<state_id_t>& stack
					 , valvec<ColorID>& color
					 , OnFinish on_finish
					 ) const {
	// multiple pass non-recursive topological sort
	assert(color.size() >= total_states()+1);
	assert(stack.size() >= 1); // could has multiple roots
	auto on_back_edge = [&](state_id_t parent, state_id_t child) {
		fstring func = BOOST_CURRENT_FUNCTION;
		if (parent == child)
			throw std::logic_error(func + ": self-loop");
		else
			throw std::logic_error(func + ": non-self-loop");
	};
	dfs_post_order_walk(stack, color, on_finish, on_back_edge);
}

// used for multiple-pass dfs_post_order_walk start from different root
// color[0] is last ColorID sequencial number
template<class ColorID, class OnFinish, class OnBackEdge>
void dfs_post_order_walk( valvec<state_id_t>& stack
						, valvec<ColorID>& color
						, OnFinish on_finish
						, OnBackEdge on_back_edge
						) const {
	// multiple pass non-recursive post order walk
	assert(color.size() >= total_states()+1);
	assert(stack.size() >= 1); // could has multiple roots
#ifndef NDEBUG
	for (size_t i = 0; i < stack.size(); ++i)
		assert(stack[i] < total_states());
#endif
	ColorID last_id = color[0];
	while (!stack.empty()) {
		state_id_t curr = stack.back();
		assert(color[curr+1] <= last_id+2);
		if (color[curr+1] <= last_id) { // color is white
			color[curr+1]  = last_id+1; // set grey
			for_each_dest_rev(curr,
			[&,curr](state_id_t child) {
				if (color[child+1] <= last_id) {
					// color is white: tree edge
					// child may have multiple copies in stack
					// but this doesn't matter, just 1 will be expanded
					stack.push_back(child);
				}
				else if (last_id+1 == color[child+1]) {
					// color is grey: back edge
					on_back_edge(curr, child);
				}
				else { // color must be black: cross edge
					assert(last_id+2 == color[child+1]);
				}
			});
		}
		else if (last_id+1 == color[curr+1]) {
			// case Color::Grey:
			color[curr+1] = last_id+2; // set black
			on_finish(curr);
			stack.pop_back();
		}
		else {
			// case Color::Black: // forward edge
			assert(last_id+2 == color[curr+1]);
			stack.pop_back();
		}
	}
	color[0] += 2; // every walk need 3 different color id
}

template<class OnFinish>
void topological_sort2(state_id_t RootState, OnFinish on_finish) const {
	// non-recursive topological sort
	assert(RootState < total_states());
	valvec<state_id_t> stack;
	stack.reserve(1024);
	stack.push_back(RootState);
	topological_sort2<OnFinish>(stack, on_finish);
}
template<class OnFinish>
void topological_sort2(valvec<state_id_t>& stack, OnFinish on_finish) const {
	auto on_back_edge = [&](state_id_t parent, state_id_t child) {
		fstring func = BOOST_CURRENT_FUNCTION;
		if (parent == child)
			throw std::logic_error(func + ": self-loop");
		else
			throw std::logic_error(func + ": non-self-loop");
	};
	dfs_post_order_walk<OnFinish>(stack, on_finish, on_back_edge);
}
template<class OnFinish>
void topological_sort0(state_id_t RootState, OnFinish on_finish) const {
	assert(RootState < total_states());
	valvec<Color> mark(total_states(), Color::White);
	topological_sort_loop<OnFinish>(&mark[0], on_finish, RootState);
}
private:
template<class OnFinish>
void
topological_sort_loop(Color* mark, OnFinish on_finish, state_id_t curr)
const {
	mark[curr] = Color::Grey;
	for_each_dest(curr, [&](state_id_t child) {
			if (Color::White == mark[child])
				this->topological_sort_loop(mark, on_finish, child);
			else if (Color::Grey == mark[child])
				throw std::logic_error("DFA_Algorithm::topological_sort: found loop");
		});
	mark[curr] = Color::Black;
	on_finish(curr);
}



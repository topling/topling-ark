
public:
template<class BaseWalker>
class ZipWalker : public BaseWalker {
public:
	valvec<state_id_t> path; // state_move(path[j], strp[j]) == path[j+1]
	valvec<byte_t>     strp;

	void getOnePath(const MyType* au, const bm_uint_t* is_confluence) {
		state_id_t s = this->next();
		assert(!au->is_pzip(s));
		strp.resize0();
		path.resize0();
		path.push_back(s);
		if (!au->has_children(s))
			return;
		if (au->is_term(s) || au->more_than_one_child(s))
			return;
		auchar_t prev_ch = au->get_single_child_char(s);
		if (prev_ch >= 256)
			return;
		for (state_id_t i = au->get_single_child(s);
			 path.size() < 254 && !terark_bit_test(is_confluence, i); ) {
			ASSERT_isNotFree2(au, i);
			assert(!au->is_pzip(i));
			assert(this->color.is0(i));
		// 	this->color.set1(i); // not needed
			strp.push_back((byte_t)prev_ch);
			path.push_back(i);
			if (au->is_term(i) || au->more_than_one_child(i)) break;
			prev_ch = au->get_single_child_char(i);
			if (prev_ch >= 256) break; // not a byte
			i = au->get_single_child(i);
		// only term state could have null target:
			assert(i < au->total_states());
			ASSERT_isNotFree2(au, i);
		}
		strp.push_back('\0');
		strp.pop_back();
	 // state_move(path[j], strp[j]) == path[j+1]:
		assert(path.size() == strp.size() + 1);
	}
};
public:
template<class SrcDFA>
void path_zip(const SrcDFA& src, const char* WalkMethod, size_t min_zstr_len = 2) {
	size_t Root = initial_state;
	path_zip(&Root, 1, src, WalkMethod, min_zstr_len);
}
template<class SrcDFA>
void path_zip(const valvec<size_t>& roots
			, const SrcDFA& src, const char* WalkMethod, size_t min_zstr_len = 2)
{
	path_zip(roots.data(), roots.size(), src, WalkMethod, min_zstr_len);
}
template<class SrcDFA>
void path_zip(const size_t* pRoots, size_t nRoots
			, const SrcDFA& src, const char* WalkMethod, size_t min_zstr_len = 2)
{
	assert(nRoots >= 1);
	assert(0 == src.num_zpath_states());
	if (src.num_zpath_states()) {
		THROW_STD(invalid_argument, "src.zpath_states=%zd", src.num_zpath_states());
	}
	febitvec is_confluence(src.total_states(), false);
	{
		valvec<state_id_t> indg;
		src.compute_indegree(pRoots, nRoots, indg);
		for (size_t i = 0, n = indg.size(); i < n; ++i) {
			if (indg[i] > 1)
				is_confluence.set1(i);
		}
	}
	typedef BFS_GraphWalker<typename SrcDFA::state_id_t>  BFS;
	typedef DFS_GraphWalker<typename SrcDFA::state_id_t>  DFS;
	typedef PFS_GraphWalker<typename SrcDFA::state_id_t>  PFS;
	if (strcasecmp(WalkMethod, "BFS") == 0)
		path_zip_imp<SrcDFA, BFS>(pRoots, nRoots, is_confluence.bldata(), src, min_zstr_len);
	else if (strcasecmp(WalkMethod, "DFS") == 0)
		path_zip_imp<SrcDFA, DFS>(pRoots, nRoots, is_confluence.bldata(), src, min_zstr_len);
	else if (strcasecmp(WalkMethod, "PFS") == 0)
		path_zip_imp<SrcDFA, PFS>(pRoots, nRoots, is_confluence.bldata(), src, min_zstr_len);
	else {
		std::string msg = BOOST_CURRENT_FUNCTION;
		msg += ": must be one of: BFS, DFS, PFS";
		throw std::invalid_argument(msg);
	}
	this->set_is_dag(src.is_dag());
}
private:
template<class SrcDFA, class BaseWalker>
void path_zip_imp(const size_t* pRoots, size_t nRoots
				, const bm_uint_t* is_confluence, const SrcDFA& src, size_t min_zstr_len)
{
	typedef typename SrcDFA::state_id_t src_state_id_t;
	TERARK_VERIFY_EQ(0, src.num_zpath_states());
	TERARK_VERIFY_LE(nRoots, src.total_states());
//	ASSERT_isNotFree(initial_state);
	this->erase_all();
	const size_t min_compress_path_len = min_zstr_len + 1;
	const auto nil = state_id_t(-1);
	valvec<state_id_t> s2ds(src.total_states(), nil);
	typename SrcDFA::template ZipWalker<BaseWalker> walker;
	walker.resize(src.total_states());
	for(size_t i = nRoots;  i > 0; ) {
		size_t r = pRoots[--i];
		TERARK_VERIFY_LT(r, src.total_states());
		s2ds[r] = i;
		walker.putRoot(r);
	}
	size_t ds = nRoots;
	while (!walker.is_finished()) {
		walker.getOnePath(&src, is_confluence);
		auto plen = walker.path.size();
		auto head = walker.path[0];
		auto tail = walker.path.back();
		if (plen >= min_compress_path_len) {
			if (nil == s2ds[head])
				s2ds[tail] = s2ds[head] = ds++;
			else
				s2ds[tail] = s2ds[head];
		} else {
			size_t j = nil == s2ds[head] ? 0 : 1;
			for (; j < plen; ++j) s2ds[walker.path[j]] = ds++;
		}
		walker.putChildren(&src, tail);
	}
	this->resize_states(ds);
	walker.resize(src.total_states());
	for (size_t i = nRoots; i > 0; )
		walker.putRoot(pRoots[--i]);
	size_t n_zpath_states = 0;
	size_t n_total_zpath_len = 0;
	valvec<CharTarget<size_t> > allmove;
	while (!walker.is_finished()) {
		walker.getOnePath(&src, is_confluence);
		allmove.erase_all();
		src_state_id_t xparent = walker.path.back();
		src.for_each_move(xparent, [&](src_state_id_t t, auchar_t c) {
		    assert(s2ds[t] < ds);
			allmove.emplace_back(c, s2ds[t]);
		});
		adl_add_other_link(this, src, xparent, s2ds);
		auto zs = s2ds[xparent];
		this->add_all_move(zs, allmove);
		if (walker.path.size() >= min_compress_path_len) {
			n_zpath_states++;
			n_total_zpath_len += walker.strp.size();
			this->add_zpath(zs, walker.strp.data(), walker.strp.size());
		//	printf("add_zpath: %s\n", walker.strp.data());
		} else {
			const src_state_id_t* path = &walker.path[0];
			for (size_t j = 0; j < walker.strp.size(); ++j) {
				auto yparent = s2ds[path[j+0]];
				auto ychild  = s2ds[path[j+1]];
				assert(yparent < ds);
				assert(ychild  < ds);
				adl_add_other_link(this, src, path[j+0], s2ds);
				this->add_move_checked(yparent, ychild, walker.strp[j]);
				assert(!this->more_than_one_child(yparent));
			}
		}
		if (src.is_term(xparent))
			this->set_term_bit(zs);
		walker.putChildren(&src, xparent);
	}
	this->pool.shrink_to_fit();
	this->m_zpath_states = n_zpath_states;
	this->m_total_zpath_len = n_total_zpath_len;
}

public:
// dead state will also be removed
template<class SrcDFA>
void normalize(const SrcDFA& src, const char* WalkMethod) {
	size_t RootState = initial_state;
	normalize(&RootState, 1, src, WalkMethod);
}
template<class SrcDFA>
void normalize(const valvec<size_t>& roots
			 , const SrcDFA& src, const char* WalkMethod)
{
	normalize(roots.data(), roots.size(), src, WalkMethod);
}
template<class SrcDFA>
void normalize(const size_t* pRoots, size_t nRoots
			 , const SrcDFA& src, const char* WalkMethod)
{
	typedef BFS_GraphWalker<state_id_t>  BFS;
	typedef DFS_GraphWalker<state_id_t>  DFS;
	typedef PFS_GraphWalker<state_id_t>  PFS;
	if (strcasecmp(WalkMethod, "BFS") == 0)
		normalize_imp<SrcDFA, BFS>(pRoots, nRoots, src);
	else if (strcasecmp(WalkMethod, "DFS") == 0)
		normalize_imp<SrcDFA, DFS>(pRoots, nRoots, src);
	else if (strcasecmp(WalkMethod, "PFS") == 0)
		normalize_imp<SrcDFA, PFS>(pRoots, nRoots, src);
	else {
		std::string msg = BOOST_CURRENT_FUNCTION;
		msg += ": WalkMethod must be one of: BFS, DFS, PFS";
		throw std::invalid_argument(msg);
	}
	this->set_is_dag(src.is_dag());
}

private:
template<class SrcDFA, class WalkerType>
void normalize_imp(const size_t* pRoots, size_t nRoots, const SrcDFA& src) {
	typedef typename SrcDFA::state_id_t src_state_id_t;
	assert(0 == src.num_zpath_states());
	assert(nRoots < src.total_states());
	const state_id_t nil = state_id_t(-1);
	valvec<state_id_t> smap(src.total_states(), nil);
	WalkerType walker;
	walker.resize(src.total_states());
	for(size_t i = nRoots; i > 0; ) {
		size_t root = pRoots[--i];
		walker.putRoot(root);
		smap[root] = i;
	}
	size_t id = nRoots;
	while (!walker.is_finished()) {
		src_state_id_t curr = walker.next();
		if (smap[curr] == nil)
			smap[curr] = id++;
		walker.putChildren(&src, curr);
	}
	this->erase_all();
	this->resize_states(id);
	walker.resize(src.total_states());
	for(size_t i = nRoots; i > 0; ) {
		size_t root = pRoots[--i];
		walker.putRoot(root);
	}
	valvec<CharTarget<size_t> > allmove(src.get_sigma(), valvec_no_init());
	while (!walker.is_finished()) {
		src_state_id_t curr = walker.next();
		size_t n = src.get_all_move(curr, allmove.data());
		for (size_t i = 0; i < n; ++i) {
			CharTarget<size_t>& x = allmove[i];
			x.target = smap[x.target];
		}
		this->add_all_move(smap[curr], allmove.data(), n);
		if (src.is_term(curr))
			this->set_term_bit(smap[curr]);
		walker.putChildren(&src, curr);
	}
	this->shrink_to_fit();
}

public:
template<class SrcDFA>
void copy_with_pzip(const SrcDFA& src) {
	assert(int(sigma) >= int(SrcDFA::sigma));
	this->erase_all();
	this->resize_states(src.total_states());
	MatchContext ctx;
	terark::AutoFree<CharTarget<size_t> > children(512);
	this->m_is_dag = src.is_dag();
	this->m_kv_delim = src.kv_delim();
	this->m_dyn_sigma = src.get_sigma();
	this->m_atom_dfa_num = src.m_atom_dfa_num;
	this->m_dfa_cluster_num = src.m_dfa_cluster_num;
	m_zpath_states = 0;
	m_total_zpath_len = 0;
	for (size_t i = 0; i < this->total_states(); ++i) {
		if (src.is_free(i)) {
			this->del_state(i);
			continue;
		}
		size_t cnt = src.get_all_move(i, children.p);
		this->add_all_move(i, children.p, cnt);
		if (src.is_pzip(i)) {
			auto zstr = src.get_zpath_data(i, &ctx);
			this->add_zpath(i, zstr.udata(), zstr.size());
			m_zpath_states++;
			m_total_zpath_len += zstr.size();
		}
		if (src.is_term(i))
			this->set_term_bit(i);
	}
	assert(src.total_zpath_len() == m_total_zpath_len);
	assert(src.num_zpath_states() == m_zpath_states);
}



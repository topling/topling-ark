
/////////////////////////////////////////////////////////////////////
template<class SrcDFA, class SrcID>
size_t dfa_union_n(const SrcDFA& src, const valvec<SrcID>& roots,
		size_t PowerLimit = size_t(-1), size_t timeout_us = 0) {
	return dfa_union_n(src, roots.data(), roots.size(), PowerLimit, timeout_us);
}
template<class SrcDFA, class SrcID>
size_t dfa_union_n(const SrcDFA& src, const SrcID* roots, size_t n_roots,
				   size_t PowerLimit = size_t(-1), size_t timeout_us = 0) {
	assert(src.get_sigma() <= this->get_sigma());
	terark::profiling clock;
	uint64_t time_start = timeout_us ? clock.now() : 0;
	this->erase_all();
	smallmap<NFA_SubSet<SrcID> > next_sets(src.get_sigma());
	valvec<CharTarget<size_t> > children(src.get_sigma(), valvec_no_init());
	SubSetHashMap<SrcID> pmap(src.total_states());
	pmap.find_or_add_subset(roots, n_roots);
	for(size_t bfshead = 0; bfshead < pmap.size(); ++bfshead) {
		if (timeout_us && (bfshead & 255) == 0) { // timeout check is expensive
			uint64_t time_curr = clock.now();
			if (uint64_t(clock.us(time_start, time_curr)) > timeout_us) {
				return 0; // Failed because timeout
			}
		}
		size_t sub_beg = pmap.node[bfshead+0].index;
		size_t sub_end = pmap.node[bfshead+1].index;
		bool b_is_final = false;
		children.erase_all();
		next_sets.resize0();
		for(size_t i = sub_beg; i < sub_end; ++i) {
			size_t parent = pmap.data[i];
			src.for_each_move(parent, [&](SrcID child, auchar_t ch) {
				next_sets.bykey(ch).push_back(child);
			});
			b_is_final |= src.is_term(parent);
		}
		for(size_t i = 0; i < next_sets.size(); ++i) {
			auto& ss = next_sets.byidx(i);
			size_t next = pmap.find_or_add_subset_sort_uniq(ss);
			children.push_back(CharTarget<size_t>(ss.ch, next));
		}
		if (pmap.data.size() > PowerLimit) {
			return 0;
		//	THROW_STD(runtime_error, "pmap.data.size=%zd exceed PowerLimit=%zd"
		//			, pmap.data.size(), PowerLimit);
		}
		if (!children.empty()) {
			std::sort(children.begin(), children.end(), CharTarget_By_ch());
			this->resize_states(pmap.size());
			this->add_all_move(bfshead, children);
		}
		if (b_is_final) this->set_term_bit(bfshead);
	}
	return pmap.data.size();
}

// return unioned_root_state_id
template<class Uint>
size_t dfa_lazy_union_n(const Uint* roots, size_t n_roots,
	   	size_t PowerLimit = size_t(-1)) {
	BOOST_STATIC_ASSERT(sizeof(Uint) >= sizeof(state_id_t));
	valvec<state_id_t> my_roots(n_roots, valvec_no_init());
	for (size_t i = 0; i < n_roots; ++i) {
		assert(roots[i] < this->total_states());
		my_roots[i] = roots[i];
	}
	return dfa_lazy_union_n(my_roots.data(), n_roots, PowerLimit);
}
template<class Uint>
size_t dfa_lazy_union_n(const valvec<Uint>& roots,
	   	size_t PowerLimit = size_t(-1)) {
	return dfa_lazy_union_n(roots.data(), roots.size(), PowerLimit);
}
size_t dfa_lazy_union_n(const state_id_t* roots, size_t n_roots,
	   	size_t PowerLimit = size_t(-1)) {
#ifndef NDEBUG
	for (size_t i = 0; i < n_roots; ++i) {
		assert(roots[i] < this->total_states());
		assert(!this->is_free(roots[i]));
		assert(!this->is_pzip(roots[i]));
	}
#endif
	smallmap<NFA_SubSet<state_id_t> > next_sets(this->get_sigma());
	valvec<CharTarget<size_t> > children(this->get_sigma(), valvec_no_init());
	size_t UnionRoot = this->total_states();
	SubSetHashMap<state_id_t> pmap(4096);
	pmap.find_or_add_subset(roots, n_roots);
	for(size_t bfshead = 0; bfshead < pmap.size(); ++bfshead) {
		size_t sub_beg = pmap.node[bfshead+0].index;
		size_t sub_end = pmap.node[bfshead+1].index;
		bool b_is_final = false;
		children.erase_all();
		next_sets.resize0();
		for(size_t i = sub_beg; i < sub_end; ++i) {
			size_t parent = pmap.data[i];
			for_each_move(parent, [&](state_id_t child, auchar_t ch) {
				next_sets.bykey(ch).push_back(child);
			});
			b_is_final |= this->is_term(parent);
		}
		for(size_t i = 0; i < next_sets.size(); ++i) {
			auto& ss = next_sets.byidx(i);
			size_t next = ss.size() == 1
						? ss[0] : UnionRoot + pmap.find_or_add_subset_sort_uniq(ss);
			children.push_back(CharTarget<size_t>(ss.ch, next));
		}
		if (pmap.data.size() > PowerLimit) {
			THROW_STD(runtime_error, "pmap.data.size=%zd exceed PowerLimit=%zd"
					, pmap.data.size(), PowerLimit);
		}
		if (!children.empty()) {
			std::sort(children.begin(), children.end(), CharTarget_By_ch());
			this->resize_states(UnionRoot + pmap.size());
			this->add_all_move(UnionRoot + bfshead, children);
		}
		if (b_is_final) this->set_term_bit(UnionRoot + bfshead);
	}
	return UnionRoot;
}

template<class SrcDFA, class SrcID>
size_t dfa_intersection_n(const SrcDFA& src, const valvec<SrcID>& roots,
	   	size_t PowerLimit = size_t(-1), size_t timeout_us = 0) {
	return dfa_intersection_n(src, roots.data(), roots.size(), PowerLimit, timeout_us);
}
template<class SrcDFA, class SrcID>
size_t
dfa_intersection_n(const SrcDFA& src, const SrcID* roots, size_t n_roots,
		size_t PowerLimit = size_t(-1), size_t timeout_us = 0) {
	assert(src.get_sigma() <= this->get_sigma());
	terark::profiling clock;
	uint64_t time_start = timeout_us ? clock.now() : 0;
	this->erase_all();
	smallmap<NFA_SubSet<SrcID> > next_sets(src.get_sigma());
	valvec<CharTarget<size_t> > children(src.get_sigma(), valvec_no_init());
	SubSetHashMap<SrcID> pmap(total_states());
	pmap.find_or_add_subset(roots, n_roots);
	for(size_t bfshead = 0; bfshead < pmap.size(); ++bfshead) {
		if (timeout_us && (bfshead & 255) == 0) { // timeout check is expensive
			uint64_t time_curr = clock.now();
			if (uint64_t(clock.us(time_start, time_curr)) > timeout_us) {
				return 0; // Failed because timeout
			}
		}
		size_t sub_beg = pmap.node[bfshead+0].index;
		size_t sub_end = pmap.node[bfshead+1].index;
		size_t sub_len = sub_end - sub_beg;
		assert(sub_len > 0);
		bool b_is_final = true;
		children.erase_all();
		next_sets.resize0();
		for(size_t i = sub_beg; i < sub_end; ++i) {
			size_t parent = pmap.data[i];
			src.for_each_move(parent, [&](SrcID child, auchar_t ch) {
				next_sets.bykey(ch).push_back(child);
			});
			b_is_final &= src.is_term(parent);
		}
		for(size_t i = 0; i < next_sets.size(); ++i) {
			auto& ss = next_sets.byidx(i);
			if (ss.size() == sub_len) {
				// ss may shrink after (unique + trim)
				size_t next = pmap.find_or_add_subset_sort_uniq(ss);
				children.push_back(CharTarget<size_t>(ss.ch, next));
			}
		}
		if (pmap.data.size() > PowerLimit) {
			THROW_STD(runtime_error, "pmap.data.size=%zd exceed PowerLimit=%zd"
					, pmap.data.size(), PowerLimit);
		}
		if (!children.empty()) {
			std::sort(children.begin(), children.end(), CharTarget_By_ch());
			this->resize_states(pmap.size());
			this->add_all_move(bfshead, children);
		}
		if (b_is_final) this->set_term_bit(bfshead);
	}
	return pmap.data.size();
}

template<class Src1, class Src2>
size_t
dfa_union(const Src1& s1, const Src2& s2, size_t PowerLimit = size_t(-1)) {
	return dfa_union(s1, initial_state, s2, initial_state, PowerLimit);
}
template<class Src1, class Src2>
size_t
dfa_union(const Src1& s1, size_t r1, const Src2& s2, size_t r2,
		  size_t PowerLimit = size_t(-1)) {
    return dfa_union(s1, r1, s1.total_states(),
                     s2, r2, s2.total_states(), PowerLimit);
}


// @param m1, m2 are used for reserve memory, dfa rooted at r1/r2 may be
//        just a small subset of s1/s2, so we can not reserve memory by
//        max(s1.total_states(), s2.total_states())!
//        caller should ensure m1/m2 are real size of dfa1/dfa2.
//         --- This was a performance BUG, fix by adding m1/m2
template<class Src1, class Src2>
size_t
dfa_union(const Src1& s1, size_t r1, size_t m1,
          const Src2& s2, size_t r2, size_t m2,
		  size_t PowerLimit = size_t(-1)) {
	assert(r1 < s1.total_states());
	assert(r2 < s2.total_states());
	assert(m1 <= s1.total_states());
	assert(m2 <= s2.total_states());
	this->erase_all();
	const size_t sigma = std::max(s1.get_sigma(), s2.get_sigma());
	assert(sigma <= this->get_sigma());
	typedef typename Src1::state_id_t ID1;
	typedef typename Src2::state_id_t ID2;
	typedef std::pair<ID1, ID2> KeyType;
	typedef uint_pair_hash_equal<ID1, ID2> KeyHashEqual;
	terark::AutoFree<CharTarget<size_t> > children1(s1.get_sigma());
	terark::AutoFree<CharTarget<size_t> > children2(s2.get_sigma());
	terark::AutoFree<CharTarget<size_t> > children3(sigma);
	gold_hash_tab<KeyType, KeyType, KeyHashEqual> pmap;
//	pmap.reserve(std::max(m1, m2));
	pmap.reserve(m1 + m2);
	pmap.insert_i(KeyType(ID1(r1), ID2(r2)));
	for(size_t bfshead = 0; bfshead < pmap.end_i(); ++bfshead) {
		KeyType parent = pmap.elem_at(bfshead);
		size_t p1 = parent.first;
		size_t p2 = parent.second;
		size_t n1 = ID1(-1) == p1 ? 0 : s1.get_all_move(p1, children1);
		size_t n2 = ID2(-1) == p2 ? 0 : s2.get_all_move(p2, children2);
		size_t i = 0, j = 0, k = 0;
		for (; i < n1 && j < n2; ++k) {
			CharTarget<size_t> ct1 = children1[i];
			CharTarget<size_t> ct2 = children2[j];
			if (ct1.ch == ct2.ch) {
				KeyType key(ID1(ct1.target), ID2(ct2.target));
				size_t  next = pmap.insert_i(key).first;
				children3[k] = CharTarget<size_t>(ct1.ch, next);
				++i; ++j;
			} else if (ct1.ch < ct2.ch) {
				KeyType key(ID1(ct1.target), ID2(-1));
				size_t  next = pmap.insert_i(key).first;
				children3[k] = CharTarget<size_t>(ct1.ch, next);
				++i;
			} else {
				KeyType key(ID1(-1), ID2(ct2.target));
				size_t  next = pmap.insert_i(key).first;
				children3[k] = CharTarget<size_t>(ct2.ch, next);
				++j;
			}
		}
		for (; i < n1; ++i, ++k) {
			CharTarget<size_t> ct1 = children1[i];
			KeyType key(ID1(ct1.target), ID2(-1));
			size_t  next = pmap.insert_i(key).first;
			children3[k] = CharTarget<size_t>(ct1.ch, next);
		}
		for (; j < n2; ++j, ++k) {
			CharTarget<size_t> ct2 = children2[j];
			KeyType key(ID1(-1), ID2(ct2.target));
			size_t  next = pmap.insert_i(key).first;
			children3[k] = CharTarget<size_t>(ct2.ch, next);
		}
		if (pmap.size()*2 > PowerLimit) {
			//THROW_STD(runtime_error, "pmap.size=%zd exceed PowerLimit=%zd"
			//		, pmap.size(), PowerLimit);
			return 0; // fail
		}
		if (k) {
			this->resize_states(pmap.end_i());
			this->add_all_move(bfshead, children3, k);
		}
		if ((ID1(-1)!=p1 && s1.is_term(p1)) || (ID2(-1)!=p2 && s2.is_term(p2)))
			this->set_term_bit(bfshead);
	}
//	this->shrink_to_fit();
	return pmap.end_i() * 2;
}

template<class Src1, class Src2>
size_t
dfa_intersection(const Src1& s1, const Src2& s2, size_t PowerLimit = size_t(-1)) {
	return dfa_intersection(s1, initial_state, s2, initial_state, PowerLimit);
}
template<class Src1, class Src2>
size_t
dfa_intersection(const Src1& s1, size_t r1, const Src2& s2, size_t r2,
	   	         size_t PowerLimit = size_t(-1)) {
    return dfa_intersection(s1, r1, s1.total_states(),
                            s2, r2, s2.total_states(), PowerLimit);
}
template<class Src1, class Src2>
size_t
dfa_intersection(const Src1& s1, size_t r1, size_t m1,
                 const Src2& s2, size_t r2, size_t m2,
	   	         size_t PowerLimit = size_t(-1)) {
	assert(r1 < s1.total_states());
	assert(r2 < s2.total_states());
	assert(m1 <= s1.total_states());
	assert(m2 <= s2.total_states());
	this->erase_all();
	MyType dst;
	size_t power_size;
  {
	const size_t sigma = std::min(s1.get_sigma(), s2.get_sigma());
	assert(sigma <= this->get_sigma());
	typedef typename Src1::state_id_t ID1;
	typedef typename Src2::state_id_t ID2;
	typedef std::pair<ID1, ID2> KeyType;
	typedef uint_pair_hash_equal<ID1, ID2> KeyHashEqual;
	terark::AutoFree<CharTarget<size_t> > children1(s1.get_sigma());
	terark::AutoFree<CharTarget<size_t> > children2(s2.get_sigma());
	terark::AutoFree<CharTarget<size_t> > children3(sigma);
	gold_hash_tab<KeyType, KeyType, KeyHashEqual> pmap;
//	pmap.reserve(std::max(m1, m2));
	pmap.reserve(m1 + m2);
	pmap.insert_i(KeyType(ID1(r1), ID2(r2)));
	for(size_t bfshead = 0; bfshead < pmap.end_i(); ++bfshead) {
		KeyType parent = pmap.elem_at(bfshead);
		size_t n1 = s1.get_all_move(parent.first , children1);
		size_t n2 = s2.get_all_move(parent.second, children2);
		size_t i = 0, j = 0, k = 0;
		while (i < n1 && j < n2) {
			CharTarget<size_t> ct1 = children1[i];
			CharTarget<size_t> ct2 = children2[j];
			if (ct1.ch == ct2.ch) {
				KeyType key(ID1(ct1.target), ID2(ct2.target));
				size_t  next = pmap.insert_i(key).first;
				children3[k] = CharTarget<size_t>(ct1.ch, next);
				++i; ++j; ++k;
			}
		   	else if (ct1.ch < ct2.ch) ++i;
			else ++j;
		}
		if (pmap.size()*2 > PowerLimit) {
			THROW_STD(runtime_error, "pmap.size=%zd exceed PowerLimit=%zd"
					, pmap.size(), PowerLimit);
		}
		if (k) {
			dst.resize_states(pmap.end_i());
			dst.add_all_move(bfshead, children3, k);
		}
		if (s1.is_term(parent.first) && s2.is_term(parent.second))
			dst.set_term_bit(bfshead);
	}
	power_size = pmap.end_i() * 2;
  }
	this->remove_dead_states(dst);
	return power_size;
}

template<class Src1, class Src2>
size_t
dfa_difference(const Src1& s1, const Src2& s2, size_t PowerLimit=size_t(-1)) {
	return dfa_difference(s1, initial_state, s2, initial_state, PowerLimit);
}
template<class Src1, class Src2>
size_t
dfa_difference(const Src1& s1, size_t r1, const Src2& s2, size_t r2,
		       size_t PowerLimit = size_t(-1)) {
    return dfa_difference(s1, r1, s1.total_states(), s2, r2, s2.total_states(), PowerLimit);
}
template<class Src1, class Src2>
size_t
dfa_difference(const Src1& s1, size_t r1, size_t m1,
               const Src2& s2, size_t r2, size_t m2,
		       size_t PowerLimit = size_t(-1)) {
	assert(r1 < s1.total_states());
	assert(r2 < s2.total_states());
	assert(m1 <= s1.total_states());
	assert(m2 <= s2.total_states());
	this->erase_all();
	MyType dst;
	size_t power_size;
  {
	const size_t sigma = s1.get_sigma();
	assert(sigma <= this->get_sigma());
	typedef typename Src1::state_id_t ID1;
	typedef typename Src2::state_id_t ID2;
	typedef std::pair<ID1, ID2> KeyType;
	typedef uint_pair_hash_equal<ID1, ID2> KeyHashEqual;
	terark::AutoFree<CharTarget<size_t> > children1(s1.get_sigma());
	terark::AutoFree<CharTarget<size_t> > children2(s2.get_sigma());
	terark::AutoFree<CharTarget<size_t> > children3(sigma);
	gold_hash_tab<KeyType, KeyType, KeyHashEqual> pmap;
//	pmap.reserve(std::max(m1, m2));
	pmap.reserve(m1 + m2);
	pmap.insert_i(KeyType(ID1(r1), ID2(r2)));
	for(size_t bfshead = 0; bfshead < pmap.end_i(); ++bfshead) {
		KeyType parent = pmap.elem_at(bfshead);
		size_t p1 = parent.first;
		size_t p2 = parent.second;
		size_t n1 = s1.get_all_move(p1, children1);
		size_t n2 = ID2(-1) == p2 ? 0 : s2.get_all_move(p2, children2);
		size_t i = 0, j = 0, k = 0;
		while (i < n1 && j < n2) {
			CharTarget<size_t> ct1 = children1[i];
			CharTarget<size_t> ct2 = children2[j];
			if (ct1.ch == ct2.ch) {
				KeyType key(ID1(ct1.target), ID2(ct2.target));
				size_t  next = pmap.insert_i(key).first;
				children3[k] = CharTarget<size_t>(ct1.ch, next);
				++i; ++j; ++k;
			}
		   	else if (ct1.ch < ct2.ch) {
				KeyType key(ID1(ct1.target), ID2(-1));
				size_t  next = pmap.insert_i(key).first;
				children3[k] = CharTarget<size_t>(ct1.ch, next);
				++i; ++k;
			}
			else ++j;
		}
		for (; i < n1; ++i, ++k) {
			CharTarget<size_t> ct1 = children1[i];
			KeyType key(ID1(ct1.target), ID2(-1));
			size_t  next = pmap.insert_i(key).first;
			children3[k] = CharTarget<size_t>(ct1.ch, next);
		}
		if (pmap.size()*2 > PowerLimit) {
			THROW_STD(runtime_error, "pmap.size=%zd exceed PowerLimit=%zd"
					, pmap.size(), PowerLimit);
		}
		if (k) {
			dst.resize_states(pmap.end_i());
			dst.add_all_move(bfshead, children3, k);
		}
		if (s1.is_term(p1) && (ID2(-1) == p2 || !s2.is_term(p2)))
			dst.set_term_bit(bfshead);
	}
	power_size = pmap.end_i() * 2;
  }
	this->remove_dead_states(dst);
	return power_size;
}

template<class DFA2>
const MyType& operator|=(const DFA2& y) {
	if (y.total_states() == 0) {
		return *this; // do nothing
	}
	else if (this->total_states() == 0) {
		this->fast_copy(y);
		return *this;
	}
	MyType z; z.dfa_union(*this, y);
	this->swap(z);
	return *this;
}

template<class DFA2>
const MyType& operator&=(const DFA2& y) {
	if (y.total_states() == 0) {
		this->erase_all();
		return *this;
	}
	else if (this->total_states() == 0) {
		return *this;
	}
	MyType z; z.dfa_intersection(*this, y);
	this->swap(z);
	return *this;
}

template<class DFA2>
const MyType& operator-=(const DFA2& y) {
	if (y.total_states() == 0 || this->total_states() == 0) {
		return *this;
	}
	MyType z; z.dfa_difference(*this, y);
	this->swap(z);
	return *this;
}

template<class DFA2>
auto operator|(const DFA2& y) const
-> std::remove_reference_t<decltype(y.total_states(), std::declval<MyType>())>
{
	MyType z;
	if (y.total_states() == 0) {
		z = *this;
	}
	else if (this->total_states() == 0) {
		if constexpr (std::is_same_v<MyType, DFA2>) {
			z = y;
		} else {
			z.erase_all();
			z.fast_copy(y);
		}
	}
	else {
		z.dfa_union(*this, y);
	}
	return z;
}

template<class DFA2>
auto operator&(const DFA2& y) const
-> std::remove_reference_t<decltype(y.total_states(), std::declval<MyType>())>
{
	MyType z;
	if (y.total_states() == 0 || this->total_states() == 0) {
		z.erase_all();
	} else {
		z.dfa_intersection(*this, y);
	}
	return z;
}

template<class DFA2>
auto operator-(const DFA2& y) const
-> std::remove_reference_t<decltype(y.total_states(), std::declval<MyType>())>
{
	MyType z;
	if (y.total_states() == 0) {
		z = *this;
	}
	else if (this->total_states() == 0) {
		z.erase_all();
	}
	else {
		z.dfa_difference(*this, y);
	}
	return z;
}



void make_edit_distance_dfa(fstring str, size_t max_ed) {
	MyType dfa;
	NFA_BaseOnDFA<MyType> nfa(&dfa);
	make_edit_distance_dfa(str, max_ed, &nfa);
}

//! @params nfa working buffered nfa, should be used for reduce memory alloc
void make_edit_distance_dfa(fstring str, size_t max_ed, NFA_BaseOnDFA<MyType>* nfa) {
	if (max_ed > str.size()) {
		max_ed = str.size();
	}
	MyType* dfa = nfa->dfa;
	dfa->erase_all();
	dfa->resize_states((str.size() + 1) * (max_ed + 1));
	nfa->erase_all();
	for(size_t ed = 0, i = 0; ed <= max_ed; ++ed) {
		size_t root = (str.size() + 1) * ed;
		for(size_t j = i; j < str.size(); ++j)
			dfa->add_move(root + j, root + j + 1, str[j]);
		dfa->set_term_bit(root + str.size());
		i += terark::utf8_byte_count(str[i]);
	}
	for(size_t ed = 0, i = 0; ed < max_ed; ++ed) {
		size_t root = (str.size() + 1) * ed;
		for(size_t j = i; j < str.size(); ) {
			size_t k = j + terark::utf8_byte_count(str[j]);
			nfa->push_epsilon(root + j, root + str.size() + 1 + k);
			j = k;
		}
		i += terark::utf8_byte_count(str[i]);
	}
	this->erase_all();
	this->set_is_dag(true);
	nfa->make_dfa(this);
}

// when max_ed==1, try to swap adjacent utf8 chars
void make_edit_distance_dfa_ex(fstring str, size_t max_ed, NFA_BaseOnDFA<MyType>* nfa) {
	make_edit_distance_dfa(str, max_ed, nfa);
	if (1 == max_ed) {
		size_t ulen = 0;
		for (size_t i = 0; i < str.size(); ++ulen) {
			i += terark::utf8_byte_count(str[i]);
		}
		if (ulen >= 2) {
			valvec<char> ed_word(str.size(), valvec_no_init());
			MinADFA_OnflyBuilder<MyType> onfly(nfa->dfa);
			size_t i = 0, j = terark::utf8_byte_count(str[0]);
			for(size_t u = 1; u < ulen; ++u) {
				size_t c = terark::utf8_byte_count(str[j]);
				ed_word.assign(str.p, str.n);
				char buf[16];
				memcpy(buf+0, &ed_word[j], c);
				memcpy(buf+c, &ed_word[i], j-i);
				memcpy(&ed_word[i], buf, c+j-i);
				onfly.add_word(ed_word);
				i = j;
			   	j += c;
			}
			onfly.close();
			MyType res;
			this->adfa_minimize(res);
			this->dfa_union(res, *nfa->dfa);
		}
	}
}

/*

void make_edit_distance_dfa(fstring str, size_t max_ed) {
	this->erase_all();
	typedef unsigned SrcID;
	valvec<SrcID> uni_offsets(str.size()+1, valvec_resreve());
	valvec<SrcID> uni_nthchar(str.size()+0, valvec_no_init());
	smallmap<NFA_SubSet<SrcID> > next_sets(256);
	terark::AutoFree<CharTarget<size_t> > children(256);
	SubSetHashMap<SrcID> pmap(str.size() * (max_ed + 2));
	for(size_t i = 0; i < str.size(); ) {
		size_t j = i + terark::utf8_byte_count(str[i]);
		for (size_t k = i; k < j; ++k) uni_nthchar[k] = uni_offsets.size();
		uni_offsets.push_back(i);
		i = j;
	}
	uni_offsets.push_back(str.size());
	const SrcID bits = terark_bsr_u32(str.size() + 2) + 1;
	const SrcID mask = (1 << bits) - 1;
	{
		SrcID root = 0;
		pmap.find_or_add_subset(&root, 1);
	}
	for(size_t bfshead = 0; bfshead < pmap.size(); ++bfshead) {
		size_t sub_beg = pmap.node[bfshead+0].index;
		size_t sub_end = pmap.node[bfshead+1].index;
		bool b_is_final = false;
		next_sets.resize0();
		for(size_t i = sub_beg; i < sub_end; ++i) {
			size_t parent = pmap.data[i];
			size_t pos = parent & mask;
			size_t ed = parent >> bits;
			size_t uidx = uni_nthchar[pos];
			printf("i=%zd pos=%zd ed=%zd\n", i, pos, ed);
			if (pos < str.size()) {
				next_sets.bykey(str[pos]).push_back(parent + 1);
				while (ed <= max_ed && pos < str.size()) {
					size_t eps_target = ((ed+1) << bits) | (pos + 1);
					next_sets.bykey(str[pos]).push_back(eps_target);
					pos += terark::utf8_byte_count(str[pos]);
					ed += 1;
				}
			} else
				b_is_final = true;
		}
		printf("next_sets.size()=%zd\n", next_sets.size());
		for(size_t i = 0; i < next_sets.size(); ++i) {
			auto& ss = next_sets.byidx(i);
			size_t next = pmap.find_or_add_subset_sort_uniq(ss);
			children[i] = CharTarget<size_t>(ss.ch, next);
		}
		if (next_sets.size() == 0) {
			std::sort(children.p, children.p + next_sets.size(), CharTarget_By_ch());
			this->resize_states(pmap.size());
			this->add_all_move(bfshead, children, next_sets.size());
		}
		if (b_is_final) this->set_term_bit(bfshead);
	}
}
*/

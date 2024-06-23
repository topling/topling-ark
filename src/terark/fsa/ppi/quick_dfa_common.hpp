public:
	template<class OP>
	void for_each_dest(state_id_t curr, OP op) const {
		assert(curr < states.size());
		const State* lstates = states.data();
		state_id_t dest = lstates[curr].u.s.dest;
		if (nil_state != dest) op(dest);
		if (lstates[curr].u.s.last_bit) return;
		op(lstates[curr+1].u.t.dest);
		int bn64 = lstates[curr+1].u.t.bn64;
		if (0 == bn64) return;
		auto bmbeg = lstates[curr+2].u.z.data;
		auto trans = bmbeg + bmbytes(bn64);
		auto tsize = bmtrans_num(bn64, bmbeg);
		for(int k = 0; k < tsize; ++k)
			op(trans_array::unaligned_get0(trans, k));
	}

	template<class OP>
	void for_each_dest_rev(state_id_t curr, OP op) const {
		assert(curr < states.size());
		const State* lstates = states.data();
		if (lstates[curr].u.s.last_bit) {
			state_id_t dest = lstates[curr].u.s.dest;
			if (nil_state != dest) op(dest);
			return;
		}
		int bn64 = lstates[curr+1].u.t.bn64;
		if (bn64) {
			auto bmbeg = lstates[curr+2].u.z.data;
			auto trans = bmbeg + bmbytes(bn64);
			auto tsize = bmtrans_num(bn64, bmbeg);
			for (int k = tsize-1; k >= 0; --k)
				op(trans_array::unaligned_get0(trans, k));
		}
		op(lstates[curr+1].u.s.dest);
		op(lstates[curr+0].u.t.dest);
	}

	fstring get_zpath_data(size_t s, MatchContext*) const {
		assert(states[s].u.s.pzip_bit);
		const byte_t* zptr = NULL;
		if (states[s].u.s.last_bit)
			zptr = states[s+1].u.z.data;
		else {
			int bn64 = states[s+1].u.t.bn64;
			if (bn64) {
				zptr = states[s+2].u.z.data +
						bmbytes(bn64) + state_id_bytes *
						bmtrans_num(bn64, states[s+2].u.z.data);
			} else {
				zptr = states[s+2].u.z.data;
			}
		}
		return fstring((const char*)zptr+1, *zptr);
	}

private:
	enum { state_id_bits_aligned = (state_id_bits + 7) & ~7 };
	enum { state_id_bytes = state_id_bits_aligned / 8 };
	typedef bitfield_array<state_id_bits_aligned> trans_array;

	template<class Walker, class Au>
	void build_from_aux(const Au& au, typename Au::state_id_t root) {
		typedef typename Au::state_id_t au_state_id_t;
		valvec<au_state_id_t> map;
		terark::AutoFree<CharTarget<size_t> > children(au.get_sigma());
		map.resize_no_init(au.total_states());
		size_t idx = 0;
		Walker walker;
		walker.resize(au.total_states());
		walker.putRoot(root);
		while (!walker.is_finished()) {
			au_state_id_t curr = walker.next();
			map[curr] = idx;
			size_t n_child = au.get_all_move(curr, children.p);
			if (n_child && children[n_child-1].ch >= Sigma) {
				auchar_t ch = children[n_child-1].ch;
				THROW_STD(out_of_range, "children[n-1=%zd] = %03d (0x%03X)"
						, n_child-1, ch, ch);
			}
			if (n_child <= 2) {
				if (au.is_pzip(curr))
					idx += slots_of_bytes(au.get_zpath_data(curr, NULL).size() + 1);
				idx += (0 == n_child) ? 1 : n_child;
			} else {
				// minch in bitmap is (children[1].ch + 1), so:
				int bits = children[n_child-1].ch - children[1].ch;
				int bn64 = bn64_of_bits(bits);
				if (bn64 > 7) THROW_STD(out_of_range, "bn64=%d", bn64);
				size_t trans_bytes = state_id_bytes * (n_child - 2);
				size_t zlen = au.is_pzip(curr) ? au.get_zpath_data(curr, NULL).size()+1 : 0;
				idx += 2 + slots_of_bytes(bmbytes(bn64) + trans_bytes + zlen);
			}
			walker.putChildren(&au, curr);
		}
		if (idx >= max_state) {
			THROW_STD(out_of_range, "idx=%zd exceeds max_state=%zd"
					, idx, (size_t)max_state);
		}
		states.resize_no_init(idx);
		walker.resize(au.total_states());
		walker.putRoot(root);
		idx = 0;
		State* lstates = states.data();
		while (!walker.is_finished()) {
			au_state_id_t curr = walker.next();
			assert(idx == map[curr]);
			size_t n_child = au.get_all_move(curr, children.p);
			lstates[idx].u.s.ch   = n_child ?	  children[0].ch	  : 0;
			lstates[idx].u.s.dest = n_child ? map[children[0].target] : nil_state;
			lstates[idx].u.s.last_bit = n_child <= 1 ? 1 : 0;
			lstates[idx].u.s.pzip_bit = au.is_pzip(curr) ? 1 : 0;
			lstates[idx].u.s.term_bit = au.is_term(curr) ? 1 : 0;
			idx++;
			if (n_child >= 2) {
				// minch in bitmap is (children[1].ch + 1), so:
				int bits = children[n_child-1].ch - children[1].ch;
				int bn64 = bn64_of_bits(bits);
				assert(bn64 <= 7);
				lstates[idx].u.t.ch   =		children[1].ch;
				lstates[idx].u.t.dest = map[children[1].target];
				lstates[idx].u.t.bn64 = bn64;
				if (n_child > 2) {
					assert(0 != bn64);
					auto minch = children[1].ch + 1;
					auto bmbeg = lstates[idx+1].u.z.data;
					auto trans = bmbeg + bmbytes(bn64);
					size_t trans_bytes = state_id_bytes * (n_child - 2);
					size_t bmtra_bytes = bmbytes(bn64) + trans_bytes;
					memset(bmbeg, 0, bmtra_bytes);
					for(size_t j = 2; j < n_child; ++j) {
						size_t k = children[j].ch - minch;
						enum { ubits = sizeof(BmUint) * 8 };
#if defined(__GNUC__) && __GNUC__*1000 + __GNUC_MINOR__ >= 10001
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wstringop-overflow"
#endif
						unaligned_save<BmUint>(bmbeg, k/ubits,
							unaligned_load<BmUint>(bmbeg, k/ubits) | BmUint(1) << k%ubits
						);
#if defined(__GNUC__) && __GNUC__*1000 + __GNUC_MINOR__ >= 10001
#  pragma GCC diagnostic pop
#endif
					}
					for(size_t j = 2; j < n_child; ++j) {
						trans_array::unaligned_set0(trans, j-2, map[children[j].target]);
					}
					size_t zlen = 0;
					if (au.is_pzip(curr)) {
						fstring zstr = au.get_zpath_data(curr, NULL);
						zlen = zstr.size() + 1;
						memcpy(lstates[idx+1].u.z.data + bmtra_bytes, zstr.data()-1, zlen);
					}
					size_t slots = slots_of_bytes(bmtra_bytes + zlen);
					memset(lstates[idx+1].u.z.data + bmtra_bytes + zlen, 0,
						   	      StateBytes*slots - bmtra_bytes - zlen);
					idx += 1 + slots;
				} else {
					assert(0 == bn64);
					idx += 1;
					goto NoBitMapCheckPathZip;
				}
			}
			else { NoBitMapCheckPathZip:
				if (au.is_pzip(curr)) {
					fstring zstr = au.get_zpath_data(curr, NULL);
					size_t zlen = zstr.size() + 1;
					size_t zp_slots = slots_of_bytes(zlen);
					memcpy(lstates[idx].u.z.data, zstr.data()-1, zlen);
					memset(lstates[idx].u.z.data + zlen, 0,
							 StateBytes * zp_slots - zlen);
					idx += zp_slots;
				}
			}
			walker.putChildren(&au, curr);
		}
		m_zpath_states = au.num_zpath_states();
		m_total_zpath_len = au.total_zpath_len();
		m_transition_num = au.total_transitions();
		assert(states.size() == idx);
	}


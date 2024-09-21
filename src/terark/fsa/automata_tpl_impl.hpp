#pragma once
#include "automata_basic.hpp"

#if defined(__GNUC__) && __GNUC_MINOR__ + 1000 * __GNUC__ > 7000
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wclass-memaccess" // which version support?
#endif

namespace terark {

template<class State, class SuperDFA>
AutomataTpl<State, SuperDFA>::~AutomataTpl() {
	if (this->mmap_base) {
		pool.get_data_byte_vec().risk_release_ownership();
		states.risk_release_ownership();
	}
}

template<class State, class SuperDFA>
void
AutomataTpl<State, SuperDFA>::insert(State& s,
									 Transition val,
									 int oldbits,
									 int newbits,
									 int inspos,
									 int oldouts)
{
    assert(oldbits >= 32);
    assert(oldbits <= newbits);
    assert(oldbits <= sigma);
    assert(newbits <= sigma);
    assert(oldbits == 32 || oldbits % header_max::BlockBits == 0);
    assert(newbits == 32 || newbits % header_max::BlockBits == 0);
    assert(oldouts <= oldbits);
    assert(inspos >= 0);
    assert(inspos <= oldouts);
    size_t oldpos = s.getpos();
    assert(oldpos < pool.size());
    int cbOldBmp = oldbits / 8;
    int cbNewBmp = newbits / 8;
    int oldTotal = pool.align_to(cbOldBmp + (oldouts + 0) * edge_size);
    int newTotal = pool.align_to(cbNewBmp + (oldouts + 1) * edge_size);
    // after align, oldTotal may equal to newTotal
    assert(oldpos + oldTotal <= pool.size());
    if (pool.size() == oldpos + oldTotal || oldTotal == newTotal) {
        if (oldTotal != newTotal)
            pool.resize_no_init(oldpos + newTotal);
        char* me = &pool.template at<char>(oldpos);
        if (inspos != oldouts)
            // in-place need backward copy, use memmove
            memmove(me + cbNewBmp + (inspos + 1) * edge_size,
                    me + cbOldBmp + (inspos + 0) * edge_size,
                    (oldouts - inspos) * edge_size); // copy post-inspos
        ((Transition*)(me + cbNewBmp))[inspos] = val;
        if (cbOldBmp != cbNewBmp) {
            memmove(me + cbNewBmp,
                    me + cbOldBmp, // copy pre-inspos
                    inspos * edge_size);
            memset(me + cbOldBmp, 0, cbNewBmp-cbOldBmp);
        }
    } else { // is not in-place copy, memcpy is ok
        size_t newpos = pool.alloc(newTotal);
        assert(newpos != oldpos);
        char* base = &pool.template at<char>(0);
        if (cbOldBmp == cbNewBmp) {
            memcpy(base + newpos,
                    base + oldpos, // bitmap + pre-inspos
                    cbOldBmp + inspos * edge_size);
        } else {
            memcpy(base + newpos, base + oldpos, cbOldBmp); // copy bitmap
            memset(base + newpos + cbOldBmp, 0, cbNewBmp-cbOldBmp);
            memcpy(base + newpos + cbNewBmp,
                    base + oldpos + cbOldBmp,
                    inspos * edge_size); // copy pre-inspos
        }
        ((Transition*)(base + newpos + cbNewBmp))[inspos] = val;
        memcpy(base + newpos + cbNewBmp + (inspos + 1) * edge_size,
                base + oldpos + cbOldBmp + (inspos + 0) * edge_size,
                (oldouts - inspos) * edge_size); // copy post-inspos
        pool.sfree(oldpos, oldTotal);
        s.setpos(newpos);
    }
}

template<class State, class SuperDFA>
void
AutomataTpl<State, SuperDFA>::check_slots(State s, int n) {
#if defined(NDEBUG)
	TERARK_UNUSED_VAR(s);
	TERARK_UNUSED_VAR(n);
#else
    int bits = bm_real_bits(s.rlen());
    const Transition* p = &pool.template at<Transition>(s.getpos() + bits/8);
    if (is_32bitmap(bits)) {
        const header_b32& hb = pool.template at<header_b32>(s.getpos());
        int n2 = fast_popcount32(hb.b);
        assert(n2 == n);
    } else {
        const header_max& hb = pool.template at<header_max>(s.getpos());
        int n2 = hb.popcnt_aligned(bits);
        assert(n2 == n);
    }
    for (int i = 0; i < n; ++i) {
    //  assert(p[i] >= 0); // state_id_t is unsigned
		ASSERT_isNotFree(p[i]);
		assert(p[i] < (state_id_t)states.size());
    }
#endif
}

template<class State, class SuperDFA>
void
AutomataTpl<State, SuperDFA>::init() {
	this->m_kv_delim = sigma < 256 ? '\t' : 256;
	this->m_atom_dfa_num = 0;
	this->m_dfa_cluster_num = 0;
	this->m_dyn_sigma = sigma;
	this->states.erase_all();
	this->states.push_back(State()); // initial_state
	this->firstFreeState = nil_state;
	this->numFreeStates  = 0;
	this->transition_num = 0;
	this->m_zpath_states = 0;
	this->m_total_zpath_len = 0;
}

template<class State, class SuperDFA>
void
AutomataTpl<State, SuperDFA>::reserve(size_t reserved_state_num) {
	states.reserve(reserved_state_num);
	// predict pool size and reserve:
	pool.reserve(reserved_state_num * sizeof(Transition));
	init();
}

template<class State, class SuperDFA>
size_t
AutomataTpl<State, SuperDFA>::new_state() {
    size_t s;
    if (nil_state == firstFreeState) {
        assert(numFreeStates == 0);
        s = states.size();
        if (s >= State::max_state) {
            THROW_STD(runtime_error, "exceeding max_state=%zd"
					, (size_t)State::max_state);
        }
        states.push_back(State());
    } else {
        assert(numFreeStates > 0);
        numFreeStates--;
        s = firstFreeState;
        firstFreeState = states[s].get_target();
        new(&states[s])State(); // call default constructor
    }
    return s;
}

template<class State, class SuperDFA>
size_t
AutomataTpl<State, SuperDFA>::clone_state(size_t source) {
    assert(source < states.size());
    size_t y = new_state();
    const State s = states[source];
	states[y] = s;
    if (s.more_than_one_child()) {
		int bits = s.rlen();
		size_t nb;
		if (is_32bitmap(bits)) {
			int trans = fast_popcount32(pool.template at<header_b32>(s.getpos()).b);
			nb = 4 + sizeof(Transition) * trans;
			transition_num += trans;
		} else {
			const header_max& hb = pool.template at<header_max>(s.getpos());
			int trans = hb.popcnt_aligned(bits);
			nb = hb.align_bits(bits)/8 + sizeof(Transition) * trans;
			transition_num += trans;
		}
		size_t pos = pool.alloc(pool.align_to(nb));
		states[y].setpos(pos);
		memcpy(&pool.template at<byte_t>(pos), &pool.template at<byte_t>(s.getpos()), nb);
    }
	else if (s.is_pzip()) {
		// very unlikely:
		assert(0); // now, unreachable
		int nzc = pool.template at<byte_t>(sizeof(state_id_t) + s.getpos());
		size_t nb = sizeof(state_id_t) + nzc + 1;
		size_t pos = pool.alloc(pool.align_to(nb));
		memcpy(&pool.template at<byte_t>(pos), &pool.template at<byte_t>(s.getpos()), nb);
		states[y].setpos(pos);
	}
	else if (nil_state != s.get_target()) {
		transition_num += 1;
	}
	return y;
}

template<class State, class SuperDFA>
void
AutomataTpl<State, SuperDFA>::del_state(size_t s) {
	assert(0 == m_zpath_states);
    assert(s < states.size());
    assert(s != initial_state); // initial_state shouldn't be deleted
	State& x = states[s];
	assert(!x.is_pzip()); // must be non-path-zipped
	if (x.more_than_one_child()) { // has transitions in pool
		int bits = x.rlen();
        size_t pos = x.getpos();
        size_t bytes;
        if (is_32bitmap(bits)) {
            uint32_t bm = pool.template at<uint32_t>(pos);
			int trans = fast_popcount32(bm);
            bytes = 4 +  trans * sizeof(Transition);
			transition_num -= trans;
        } else {
			int trans = pool.template at<header_max>(pos).popcnt_aligned(bits);
            bytes = trans * sizeof(Transition)
                    + header_max::align_bits(bits) / 8;
			transition_num -= trans;
        }
        pool.sfree(pos, pool.align_to(bytes));
    }
	else if (x.get_target() != nil_state)
		transition_num -= 1; // just 1 transition
	// put s to free list, states[s] is not a 'State' from now
	x.set_target(firstFreeState);
	x.set_free();
	firstFreeState = s;
	numFreeStates++;
}

template<class State, class SuperDFA>
void
AutomataTpl<State, SuperDFA>::del_all_move(size_t s) {
	assert(0 == m_zpath_states);
    assert(s < states.size());
	State& x = states[s];
	assert(!x.is_pzip()); // must be non-path-zipped
	if (x.more_than_one_child()) { // has transitions in pool
		int bits = x.rlen();
        size_t pos = x.getpos();
        size_t bytes;
        if (is_32bitmap(bits)) {
            uint32_t bm = pool.template at<uint32_t>(pos);
			int trans = fast_popcount32(bm);
            bytes = 4 +  trans * sizeof(Transition);
			transition_num -= trans;
        } else {
			int trans = pool.template at<header_max>(pos).popcnt_aligned(bits);
            bytes = trans * sizeof(Transition)
                    + header_max::align_bits(bits) / 8;
			transition_num -= trans;
        }
        pool.sfree(pos, pool.align_to(bytes));
    }
	else if (x.get_target() != nil_state)
		transition_num -= 1; // just 1 transition
	x.set_target(nil_state);
	x.setch(0);
}

template<class State, class SuperDFA>
size_t
AutomataTpl<State, SuperDFA>::min_free_state() const {
	size_t s = firstFreeState;
	size_t s_min = states.size()-1;
	while (nil_state != s) {
		s_min = std::min(s, s_min);
		s = states[s].get_target();
	}
	return s_min;
}

template<class State, class SuperDFA>
typename
AutomataTpl<State, SuperDFA>::Transition
AutomataTpl<State, SuperDFA>::state_move(const State& s, auchar_t ch)
const {
    assert(s.getMinch() <= s.getMaxch());
    if (!s.range_covers(ch))
        return nil_state;
    if (!s.more_than_one_child())
		return State::first_trans(single_target(s));
    int bpos = ch - s.getMinch(); // bit position
    int bits = s.rlen();
    if (is_32bitmap(bits)) {
        const header_b32& hb = pool.template at<header_b32>(s.getpos());
        if (hb.b & uint32_t(1)<<bpos) {
            int index = fast_popcount_trail(hb.b, bpos);
            return hb.s[index];
        }
    } else {
        const header_max& hb = pool.template at<header_max>(s.getpos());
        if (hb.is1(bpos)) {
            int index = hb.popcnt_index(bpos);
            return hb.base(bits)[index];
        }
    }
    return nil_state;
}

template<class State, class SuperDFA>
void
AutomataTpl<State, SuperDFA>::add_all_move(size_t s,
										   const CharTarget<size_t>* trans,
										   size_t n)
{
	assert(s < states.size());
	State& x = states[s];
	assert(!x.is_free());
	assert(!x.is_pzip());
	assert(x.get_target() == nil_state);
	assert(n <= sigma);
	transition_num += n;
	if (0 == n) return; // do nothing
	if (1 == n) {
		x.set_target(trans[0].target);
		x.setch(trans[0].ch);
		return;
	}
#ifndef NDEBUG
	for (size_t i = 1; i < n; ++i)
		assert(trans[i-1].ch < trans[i].ch);
#endif
	auchar_t minch = trans[0].ch;
	size_t bits = bm_real_bits(trans[n-1].ch - minch + 1);
	assert(bits % 32 == 0);
	size_t size = pool.align_to(bits/8 + sizeof(transition_t)*n);
	size_t pos = pool.alloc(size);
	if (is_32bitmap(bits)) {
		uint32_t& bm = pool.template at<uint32_t>(pos);
		bm = 0;
		for (size_t i = 0; i < n; ++i) {
			auchar_t ch = trans[i].ch;
			bm |= uint32_t(1) << (ch-minch);
		}
	} else {
		header_max& bm = pool.template at<header_max>(pos);
		assert((char*)bm.base(bits) == (char*)&bm + bits/8);
		memset(&bm, 0, bits/8);
		for (size_t i = 0; i < n; ++i) {
			auchar_t ch = trans[i].ch;
			bm.set1(ch-minch);
		}
	}
	transition_t* t = &pool.template at<transition_t>(pos+bits/8);
	for (size_t i = 0; i < n; ++i) {
		new (t+i) transition_t (trans[i].target);
	}
	x.setpos(pos);
	x.setch(minch); // setMinch has false positive assert()
	x.setMaxch(trans[n-1].ch);
}

template<class State, class SuperDFA>
size_t
AutomataTpl<State, SuperDFA>::add_move_imp(size_t source,
										   size_t target,
										   auchar_t ch,
										   bool OverwriteExisted)
{
    assert(source < states.size());
    assert(target < states.size());
	ASSERT_isNotFree(source);
	ASSERT_isNotFree(target);
	assert(!states[source].is_pzip());
//	assert(!states[target].is_pzip()); // target could be path zipped
    State& s = states[source];
    assert(s.getMinch() <= s.getMaxch());
    if (!s.more_than_one_child()) {
		const size_t old_target = s.get_target();
        if (nil_state == old_target) {
        //	assert(0 == s.getMinch()); // temporary assert
            s.setch(ch);
            s.set_target(target);
			transition_num++;
			return nil_state;
        }
        if (s.getMinch() == ch) {
            // the target for ch has existed
			if (OverwriteExisted)
				s.set_target(target);
            return old_target;
        }
		auchar_t minch = s.getMinch();
		auchar_t maxch = minch;
	//	assert(0 != minch); // temporary assert
        // 's' was direct link to the single target state
        // now alloc memory in pool
        if  (s.getMinch() > ch)
                minch = ch, s.setMinch(ch);
        else maxch = ch, s.setMaxch(ch);
        int bits = bm_real_bits(s.rlen());
        size_t pos = pool.alloc(pool.align_to(bits/8 + 2 * edge_size));
        if (is_32bitmap(bits)) {
            // lowest bit must be set
            pool.template at<uint32_t>(pos) = 1 | uint32_t(1) << (maxch - minch);
        } else {
            memset(&pool.template at<char>(pos), 0, bits/8);
            pool.template at<header_max>(pos).set1(0);
            pool.template at<header_max>(pos).set1(maxch - minch);
        }
        Transition* trans = &pool.template at<Transition>(pos + bits/8);
        if (ch == minch) {
            trans[0] = target;
            trans[1] = State::first_trans(old_target);
        } else {
            trans[0] = State::first_trans(old_target);
            trans[1] = target;
        }
        s.setpos(pos);
        check_slots(s, 2);
		transition_num++;
        return nil_state;
    }
    assert(nil_state != s.get_target());
    int oldbits = bm_real_bits(s.rlen());
    int oldouts;
    if (ch < s.getMinch()) {
        int newbits = bm_real_bits(s.getMaxch() - ch + 1);
        if (is_32bitmap(newbits)) { // oldbits also == 32
            header_b32& hb = pool.template at<header_b32>(s.getpos());
            oldouts = fast_popcount32(hb.b);
            hb.b <<= s.getMinch() - ch;
            hb.b  |= 1; // insert target at beginning
            insert(s, target, oldbits, newbits, 0, oldouts);
        } else { // here newbits > 32...
            header_max* hb;
            if (is_32bitmap(oldbits)) {
                uint32_t oldbm32 = pool.template at<header_b32>(s.getpos()).b;
                oldouts = fast_popcount32(oldbm32);
                insert(s, target, oldbits, newbits, 0, oldouts);
                hb = &pool.template at<header_max>(s.getpos());
                hb->block(0) = oldbm32; // required for bigendian CPU
            } else {
                oldouts = pool.template at<header_max>(s.getpos()).popcnt_aligned(oldbits);
                insert(s, target, oldbits, newbits, 0, oldouts);
                hb = &pool.template at<header_max>(s.getpos());
            }
            hb->shl(s.getMinch() - ch, newbits/hb->BlockBits);
            hb->set1(0);
        }
        s.setMinch(ch);
        check_slots(s, oldouts+1);
    } else if (ch > s.getMaxch()) {
        int newbits = bm_real_bits(ch - s.getMinch() + 1);
        if (is_32bitmap(newbits)) { // oldbits is also 32
            header_b32& hb = pool.template at<header_b32>(s.getpos());
            oldouts = fast_popcount32(hb.b);
            hb.b |= uint32_t(1) << (ch - s.getMinch());
            insert(s, target, oldbits, newbits, oldouts, oldouts);
        } else { // now newbits > 32
            header_max* hb;
            if (is_32bitmap(oldbits)) {
                uint32_t oldbm32 = pool.template at<header_b32>(s.getpos()).b;
                oldouts = fast_popcount32(oldbm32);
                insert(s, target, oldbits, newbits, oldouts, oldouts);
                hb = &pool.template at<header_max>(s.getpos());
                hb->block(0) = oldbm32; // required for bigendian CPU
            } else {
                oldouts = pool.template at<header_max>(s.getpos()).popcnt_aligned(oldbits);
                insert(s, target, oldbits, newbits, oldouts, oldouts);
                hb = &pool.template at<header_max>(s.getpos());
            }
			hb->set1(ch - s.getMinch());
        }
        s.setMaxch(ch);
        check_slots(s, oldouts+1);
    } else { // ch is in [s.minch, s.maxch], doesn't need to expand bitmap
        int inspos;
        int bitpos = ch - s.getMinch();
        if (is_32bitmap(oldbits)) {
            header_b32& hb = pool.template at<header_b32>(s.getpos());
            oldouts = fast_popcount32(hb.b);
            inspos = fast_popcount_trail(hb.b, bitpos);
            if (hb.b & uint32_t(1) << bitpos) {
				state_id_t old = hb.s[inspos];
                if (OverwriteExisted)
					hb.s[inspos] = target;
                return old;
            }
            hb.b |= uint32_t(1) << bitpos;
        } else {
            header_max& hb = pool.template at<header_max>(s.getpos());
            oldouts = hb.popcnt_aligned(oldbits);
            inspos = hb.popcnt_index(bitpos);
            if (hb.is1(bitpos)) {
				state_id_t old = hb.base(oldbits)[inspos];
                if (OverwriteExisted)
					hb.base(oldbits)[inspos] = target;
				return old;
            }
            hb.set1(bitpos);
        }
        insert(s, target, oldbits, oldbits, inspos, oldouts);
        check_slots(s, oldouts+1);
    }
    transition_num++;
	return nil_state;
}

template<class State, class SuperDFA>
size_t
AutomataTpl<State, SuperDFA>::num_children(size_t curr) const {
    const State s = states[curr];
    if (!s.more_than_one_child()) {
        state_id_t target = single_target(s);
		return (nil_state == target) ? 0 : 1;
    }
    int bits = s.rlen(), tn;
    if (is_32bitmap(bits)) {
        const header_b32& hb = pool.template at<header_b32>(s.getpos());
        tn = fast_popcount32(hb.b);
    } else {
        const header_max& hb = pool.template at<header_max>(s.getpos());
        tn = hb.popcnt_aligned(bits);
    }
	return tn;
}

template<class State, class SuperDFA>
size_t
AutomataTpl<State, SuperDFA>::trans_bytes(const State& s) const {
    if (!s.more_than_one_child()) {
		assert(s.is_pzip());
		return sizeof(state_id_t);
	}
    int bits = s.rlen();
    if (is_32bitmap(bits)) {
        const header_b32& hb = pool.template at<header_b32>(s.getpos());
		int tn = fast_popcount32(hb.b);
		return tn * sizeof(Transition) + 4;
    } else {
        const header_max& hb = pool.template at<header_max>(s.getpos());
        int tn = hb.popcnt_aligned(bits);
		return tn * sizeof(Transition) + hb.align_bits(bits)/8;
    }
}

template<class State, class SuperDFA>
void
AutomataTpl<State, SuperDFA>::resize_states(size_t new_states_num) {
	if (new_states_num >= max_state) {
		THROW_STD(logic_error
			, "new_states_num=%zd, exceed max_state=%zd"
			, new_states_num, (size_t)max_state);
	}
    states.resize(new_states_num);
}

template<class State, class SuperDFA>
size_t AutomataTpl<State, SuperDFA>::mem_size() const {
    return pool.size() + sizeof(State) * states.size();
}

template<class State, class SuperDFA>
void AutomataTpl<State, SuperDFA>::shrink_to_fit() {
	states.shrink_to_fit();
	pool.shrink_to_fit();
}

template<class State, class SuperDFA>
void
AutomataTpl<State, SuperDFA>::swap(AutomataTpl& y) {
	assert(typeid(*this) == typeid(y));
	risk_swap(y);
}

template<class State, class SuperDFA>
void
AutomataTpl<State, SuperDFA>::risk_swap(AutomataTpl& y) {
	BaseDFA::risk_swap(y);
    pool  .swap(y.pool  );
    states.swap(y.states);
	std::swap(firstFreeState, y.firstFreeState);
	std::swap(numFreeStates , y.numFreeStates );
    std::swap(transition_num, y.transition_num);
}

template<class State, class SuperDFA>
void
AutomataTpl<State, SuperDFA>::add_zpath(state_id_t ss,
										const byte_t* str,
										size_t len)
{
    assert(len <= 255);
	assert(!states[ss].is_pzip());
	byte_t* p;
	size_t  newpos;
	if (!states[ss].more_than_one_child()) {
		newpos = pool.alloc(pool.align_to(sizeof(state_id_t) + len+1));
		pool.template at<state_id_t>(newpos) = states[ss].get_target();
		p = &pool.template at<byte_t>(newpos + sizeof(state_id_t));
	}
	else {
		size_t oldpos = states[ss].getpos();
		size_t oldlen = trans_bytes(ss);
		size_t newlen = pool.align_to(oldlen + len + 1);
		newpos = pool.alloc3(oldpos, pool.align_to(oldlen), newlen);
		p = &pool.template at<byte_t>(newpos + oldlen);
	}
	states[ss].setpos(newpos);
	p[0] = len;
	memcpy(p+1, str, len);
	states[ss].set_pzip_bit();
}

template<class State, class SuperDFA>
size_t
AutomataTpl<State, SuperDFA>::get_zipped_path_str_pos(const State& s)
const {
	assert(s.is_pzip());
	size_t xpos;
	if (!s.more_than_one_child())
		// before xpos, it is the single_target
		xpos = s.getpos() + sizeof(state_id_t);
	else
		xpos = s.getpos() + trans_bytes(s);
	return xpos;
}

template<class State, class SuperDFA>
fstring
AutomataTpl<State, SuperDFA>::get_zpath_data(const State& s)
const {
	assert(!s.is_free());
	const char*  zbeg = reinterpret_cast<const char*>(pool.data());
	const size_t zpos = get_zipped_path_str_pos(s);
	return fstring(zbeg + zpos+1, pool.byte_at(zpos));
}

template<class State, class SuperDFA>
fstring
AutomataTpl<State, SuperDFA>::get_zpath_data(size_t s, MatchContext*)
const {
	assert(s < states.size());
	assert(!states[s].is_free());
	const char*  zbeg = reinterpret_cast<const char*>(pool.data());
	const size_t zpos = get_zipped_path_str_pos(states[s]);
	return fstring(zbeg + zpos+1, pool.byte_at(zpos));
}

template<class State, class SuperDFA>
size_t
AutomataTpl<State, SuperDFA>::hopcroft_hash(size_t x_id) const {
	assert(x_id < states.size());
	const State& s = states[x_id];
	assert(!s.is_pzip());
	size_t h = s.getMinch() * 256;
	if (s.more_than_one_child()) {
		int bits = s.rlen();
		h = h * 7 + bits;
		size_t pos = s.getpos();
		int n;
		if (is_32bitmap(bits)) {
			uint32_t bm = pool.template at<uint32_t>(pos);
			h = FaboHashCombine(h, bm);
			n = fast_popcount32(bm);
		}
		else {
			const header_max& hb = pool.template at<header_max>(pos);
			bits = hb.align_bits(bits);
			for (int i = 0; i < bits/hb.BlockBits; ++i)
				h = FaboHashCombine(h, hb.block(i));
			n = hb.popcnt_aligned(bits);
		}
		h = FaboHashCombine(h, n);
	}
	return h;
}

template<class State, class SuperDFA>
size_t
AutomataTpl<State, SuperDFA>::onfly_hash_hash(size_t x_id) const {
	assert(x_id < states.size());
	const State& s = states[x_id];
	assert(!s.is_pzip());
	size_t h = s.getMinch();
	if (!s.more_than_one_child()) {
		h = FaboHashCombine(h, s.get_target());
	}
	else {
		int bits = s.rlen();
		h = h * 7 + bits;
		size_t pos = s.getpos();
		int n;
		const Transition* p;
		if (is_32bitmap(bits)) {
			uint32_t bm = pool.template at<uint32_t>(pos);
			h = FaboHashCombine(h, bm);
			p = &pool.template at<Transition>(pos + 4);
			n = fast_popcount32(bm);
		}
		else {
			const header_max& hb = pool.template at<header_max>(pos);
			bits = hb.align_bits(bits);
			for (int i = 0; i < bits/hb.BlockBits; ++i)
				h = FaboHashCombine(h, hb.block(i));
			p = &pool.template at<Transition>(pos + bits/8);
			n = hb.popcnt_aligned(bits);
		}
		for (int i = 0; i < n; ++i) {
			h = FaboHashCombine(h, static_cast<state_id_t>(p[i]));
		}
		h = FaboHashCombine(h, n);
	}
	if (s.is_term())
		h = h + 1;
	return h;
}

template<class State, class SuperDFA>
bool
AutomataTpl<State, SuperDFA>::onfly_hash_equal(size_t x_id, size_t y_id)
const {
	assert(x_id < states.size());
	assert(y_id < states.size());
	const State& x = states[x_id];
	const State& y = states[y_id];
	if (x.getMinch() != y.getMinch())
		return false;
	if (x.getMaxch() != y.getMaxch())
		return false;
	if (x.is_term() != y.is_term())
		return false;
	// now x.rlen() always equals to y.rlen()
	if (!x.more_than_one_child())
		return x.get_target() == y.get_target();
	int bits = x.rlen();
	size_t xpos = x.getpos();
	size_t ypos = y.getpos();
	//  assert(xpos != ypos);
	int n;
	const Transition *px, *py;
	if (is_32bitmap(bits)) {
		uint32_t xbm = pool.template at<uint32_t>(xpos);
		uint32_t ybm = pool.template at<uint32_t>(ypos);
		if (xbm != ybm)
			return false;
		px = &pool.template at<Transition>(xpos + 4);
		py = &pool.template at<Transition>(ypos + 4);
		n = fast_popcount32(xbm);
	}
	else {
		const header_max& xhb = pool.template at<header_max>(xpos);
		const header_max& yhb = pool.template at<header_max>(ypos);
		bits = xhb.align_bits(bits);
		n = 0;
		for (int i = 0; i < bits/xhb.BlockBits; ++i) {
			if (xhb.block(i) != yhb.block(i))
				return false;
			else
				n += fast_popcount(xhb.block(i));
		}
		px = &pool.template at<Transition>(xpos + bits/8);
		py = &pool.template at<Transition>(ypos + bits/8);
	}
	for (int i = 0; i < n; ++i) {
		state_id_t tx = px[i];
		state_id_t ty = py[i];
		if (tx != ty)
			return false;
	}
	return true;
}

template<class State, class SuperDFA>
size_t
AutomataTpl<State, SuperDFA>::adfa_hash_hash(const state_id_t* Min, size_t state_id)
const {
	const State& s = states[state_id];
	assert(!s.is_pzip());
	size_t h = s.getMinch();
	if (!s.more_than_one_child()) {
		state_id_t t = s.get_target();
		if (nil_state != t)
			h = FaboHashCombine(h, Min[t]);
	}
	else {
		const byte_t* pool = this->pool.data();
		assert(NULL != pool);
		int bits = s.rlen();
		h = h * 7 + bits;
		size_t pos = s.getpos();
		int n;
		const Transition* p;
		if (is_32bitmap(bits)) {
			uint32_t bm = (const uint32_t&)pool[pos];
			h = FaboHashCombine(h, bm);
			p = (const Transition*)(pool + pos + 4);
			n = fast_popcount32(bm);
		}
		else {
			const header_max& hb = (const header_max&)pool[pos];
			bits = hb.align_bits(bits);
			for (int i = 0; i < bits/hb.BlockBits; ++i)
				h = FaboHashCombine(h, hb.block(i));
			p = (const Transition*)(pool + pos + bits/8);
			n = hb.popcnt_aligned(bits);
		}
		for (int i = 0; i < n; ++i)
			h = FaboHashCombine(h, Min[p[i]]);
		h = FaboHashCombine(h, n);
	}
	if (s.is_term())
		h = h + 1;
	return h;
}

template<class State, class SuperDFA>
bool
AutomataTpl<State, SuperDFA>::adfa_hash_equal(const state_id_t* Min, size_t x_id, size_t y_id)
const {
	const State& x = states[x_id];
	const State& y = states[y_id];
	if (x.getMinch() != y.getMinch())
		return false;
	if (x.getMaxch() != y.getMaxch())
		return false;
	if (x.is_term() != y.is_term())
		return false;
	// now x.rlen() always equals to y.rlen()
	if (!x.more_than_one_child()) {
		state_id_t tx = x.get_target();
		state_id_t ty = y.get_target();
		assert(nil_state == tx || nil_state != Min[tx]);
		assert(nil_state == ty || nil_state != Min[ty]);
		if (nil_state == tx)
			return nil_state == ty;
		else if (nil_state == ty)
			return false;
		else
			return Min[tx] == Min[ty];
	}
	const byte_t* pool = this->pool.data();
	assert(NULL != pool);
	int bits = x.rlen();
	size_t xpos = x.getpos();
	size_t ypos = y.getpos();
	int n;
	const Transition *px, *py;
	if (is_32bitmap(bits)) {
		uint32_t xbm = *(const uint32_t*)(pool + xpos);
		uint32_t ybm = *(const uint32_t*)(pool + ypos);
		if (xbm != ybm)
			return false;
		px = (const Transition*)(pool + xpos + 4);
		py = (const Transition*)(pool + ypos + 4);
		n = fast_popcount32(xbm);
		assert(n >= 2);
	}
	else {
		const header_max& xhb = *(const header_max*)(pool + xpos);
		const header_max& yhb = *(const header_max*)(pool + ypos);
		bits = xhb.align_bits(bits);
		n = 0;
		for (int i = 0; i < bits/xhb.BlockBits; ++i) {
			if (xhb.block(i) != yhb.block(i))
				return false;
			else
				n += fast_popcount(xhb.block(i));
		}
		px = (const Transition*)(pool + xpos + bits/8);
		py = (const Transition*)(pool + ypos + bits/8);
		assert(n >= 2);
	}
	for (int i = 0; i < n; ++i) {
		state_id_t tx = px[i];
		state_id_t ty = py[i];
		assert(nil_state == tx || nil_state != Min[tx]);
		assert(nil_state == ty || nil_state != Min[ty]);
		if (Min[tx] != Min[ty])
			return false;
	}
	return true;
}

} // namespace terark

#if defined(__GNUC__) && __GNUC_MINOR__ + 1000 * __GNUC__ > 7000
  #pragma GCC diagnostic pop
#endif

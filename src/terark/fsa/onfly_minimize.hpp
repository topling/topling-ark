#pragma once

#include <terark/valvec.hpp>
#include <terark/gold_hash_idx.hpp>
#include <terark/gold_hash_map.hpp>
#include "graph_walker.hpp"
#include <terark/num_to_str.hpp>

namespace terark {

template<class Elem, class StateID>
struct FastLinkStore : valvec<Elem>, gold_hash_idx_default_bucket<StateID> {
	StateID link(size_t state_id) const {
		assert(state_id < this->size());
		return (*this)[state_id].link;
	}
	void setlink(size_t state_id, StateID linkVal) {
		assert(state_id < this->size());
		(*this)[state_id].link = linkVal;
	}
};

/*
template<int LinkBits>
class CompactBucketStore : public dummy_bucket<size_t, LinkBits> {
	uint08_t* m_bucket;
	size_t    m_bsize;
	typedef bitfield_array<LinkBits> bf_access;
public:
	typedef LinkTp link_t;
	using dummy_bucket<size_t, LinkBits>::tail;
	CompactBucketStore() {
		m_bucket = NULL;
		m_bsize = 0;
		resize_fill_bucket(53);
	}
	~CompactBucketStore() {
		if (m_bucket) ::free(m_bucket);
	}
	void resize_fill_bucket(size_t bsize) {
		if (m_bucket) ::free(m_bucket);
		size_t bytes = sizeof(LinkTp) * bsize / 8 + 12;
		m_bucket = (uint08_t*)::malloc(bytes);
		if (NULL == m_bucket) {
			throw std::bad_alloc();
		//	assert(0); // immediately fail on debug mode
		//	throw std::runtime_error("rehash failed, unrecoverable");
		}
		memset(m_bucket, -1, bytes);
		m_bsize = bsize;
	}
	void reset_bucket() {
		assert(NULL != m_bucket);
		size_t bytes = sizeof(LinkTp) * bsize / 8 + 12;
		memset(m_bucket, -1, bytes);
	}
	void setbucket(size_t pos, size_t val) {
		assert(pos < m_bsize);
	   	bf_access::template set<0>(pos, val);
   	}
	size_t bucket(size_t pos) const {
		assert(pos < m_bsize);
		return bf_access::template get<0>(m_bucket, pos);
   	}
	size_t bucket_size() const { return m_bsize; }
};

template<bool HasIndegree, int LinkBits>
class CompactLinkStore {
	bitfield_array<LinkBits> m_bf;
public:
	enum { NodeFields = HasIndegree ? 2 : 1 };
	void setlink(size_t id, size_t val) {
		assert(id < m_lsize);
		bf_access::template set<0>(m_link, NodeFields*id, val);
	}
	void link(size_t id) {
		assert(id < m_lsize);
		return bf_access::template get<0>(m_link, NodeFields*id);
	}
	void indegree_set(size_t idx, size_t val) {
		BOOST_STATIC_ASSERT(HasIndegree);
		bf_access::template set<0>(m_link, idx, val);
	}
	void indegree_inc(size_t idx) {
		BOOST_STATIC_ASSERT(HasIndegree);
		auto val = bf_access::template get<0>(m_link, idx);
		bf_access::template set<0>(m_link, idx, val+1);
	}
	void indegree_inc(size_t idx) {
		BOOST_STATIC_ASSERT(HasIndegree);
		auto val = bf_access::template get<0>(m_link, idx);
		assert(val >= 1);
		bf_access::template set<0>(m_link, idx, val-1);
	}
};
*/

template<class Au> // Use as a templatized namespace
class DFA_OnflyMinimize {
   	DFA_OnflyMinimize() = delete;
   	~DFA_OnflyMinimize() = delete;

public:
	typedef typename Au::state_t State;
	typedef typename Au::state_id_t state_id_t;
	static const state_id_t nil_state = Au::nil_state;
	static const state_id_t max_state = Au::max_state;

	class HashEq {
		const Au* au;
	public:
		explicit HashEq(const Au* a) : au(a) {}
		size_t hash(size_t x) const { return au->onfly_hash_hash(x); }
		bool equal(size_t x, size_t y) const {
		   	return au->onfly_hash_equal(x, y); }
	};

    struct Ordered_Real_Map_Register {
		// state_id_t is the value of gold_hash_tab, it need memory,
		// so it is a <<Real_Map>>, this is not preferable on buiding
		// a monolithic large MinADFA, but when building many small ADFA
		// module on a large existing DFA, this will be more faster
		// and using less memory than Parallel_Register.
		// Caller should use <<reset_root_state>> to create MinADFA modules
		typedef node_layout<state_id_t, state_id_t, FastCopy, ValueInline> Layout;
		typedef terark_identity<state_id_t> KeyExtractor;
		gold_hash_tab<state_id_t,state_id_t,HashEq,KeyExtractor,Layout> reg;
		std::pair<size_t, bool> find_or_add(size_t state_id) {
			std::pair<size_t, bool> ib = reg.insert_i(state_id);
			if (ib.second)
				ib.first = state_id; // insert success
			else // the existed equivalent state maybe state_id itself
				ib.first = reg.elem_at(ib.first); // the EqStateID
		   	return ib;
	   	}
		void erase(state_id_t state_id) { reg.erase(state_id); }
		void resize_parallel_hashtab(size_t) {}
		bool is_inserted(state_id_t s) const {
			size_t f = reg.find_i(s);
		   	return reg.end_i() != f && reg.elem_at(f) == s;
		}
		explicit Ordered_Real_Map_Register(const Au* au) : reg(HashEq(au)) {}
	};
    struct UnOrdered_Real_Map_Register : Ordered_Real_Map_Register {
		explicit UnOrdered_Real_Map_Register(const Au* au)
		   	: Ordered_Real_Map_Register(au) {}
		valvec<state_id_t> m_indegree; // m_indegree[s] may be very large
		state_id_t indegree(state_id_t s) { return m_indegree[s]; }
		void indegree_set(size_t s, size_t val) { m_indegree[s] = val; }
		void indegree_inc(size_t s) { m_indegree[s]++; }
		void indegree_dec(size_t s) { m_indegree[s]--; }
		void resize_indegree(size_t newsize) { m_indegree.resize(newsize); }
		void compute_indegree(const Au* au, state_id_t RootState) {
		   	au->compute_indegree(RootState, m_indegree); }
	};

	struct UnOrdered_Node1 {
		UnOrdered_Node1() : link(dummy_bucket<state_id_t>::delmark), indegree() {}
		state_id_t link;
		state_id_t indegree;
	};
	struct Ordered_Node1 {
		Ordered_Node1() : link(dummy_bucket<state_id_t>::delmark) {}
		state_id_t link;
	};
	enum { StateID_Bits = StaticUintBits<Au::nil_state>::value };
#pragma pack(push,1)
	struct UnOrdered_Node2 {
		static const size_t tail = (size_t(1) << StateID_Bits) - 1;
		size_t head : StateID_Bits;
		size_t next : StateID_Bits;
		size_t indegree : StateID_Bits;
	};
	struct Ordered_Node2 {
		static const size_t tail = (size_t(1) << StateID_Bits) - 1;
		size_t head : StateID_Bits;
		size_t next : StateID_Bits;
	};
#pragma pack(pop)
	template<class Node> struct RegisterTable1 {
		gold_hash_idx1<FastLinkStore<Node, state_id_t>, HashEq> reg;
		explicit RegisterTable1(Au* au)
		  : reg(FastLinkStore<Node, state_id_t>(), HashEq(au)) {}
	};
	template<class Node> struct RegisterTable2 {
		gold_hash_idx2<HashEq, Node> reg;
		explicit RegisterTable2(Au* au) : reg(HashEq(au)) {}
	};
	template<class RegisterTable>
	struct Base_Parallel_Register : RegisterTable {
		using RegisterTable::reg;
		std::pair<size_t, bool> find_or_add(state_id_t state_id) {
		//	printf("find_or_add(state_id=%u)\n", state_id);
			if (reg.is_deleted(state_id)) {
				size_t EqStateID = reg.insert_at(state_id);
				bool   Successed = EqStateID == state_id;
				return std::make_pair(EqStateID, Successed);
			}
			return std::make_pair(state_id, false);
		}
		void erase(state_id_t state_id) {
		// 	printf("erase(state_id=%u)\n", state_id);
		   	reg.erase_i(state_id);
	   	}
		void resize_parallel_hashtab(size_t n) { reg.resize(n); }
		bool is_inserted(state_id_t s) const { return !reg.is_deleted(s); }
		explicit Base_Parallel_Register(Au* au) : RegisterTable(au) {}
	};
	template<class BaseRegister>
	struct UnOrdered_Parallel_Register_tpl : BaseRegister {
		using BaseRegister::reg;
		explicit UnOrdered_Parallel_Register_tpl(Au* au) : BaseRegister(au) {}
		state_id_t indegree(size_t s) { return reg.node_at(s).indegree; }
		void indegree_set(size_t s, size_t val) { reg.node_at(s).indegree = val; }
		void indegree_inc(size_t s) { reg.node_at(s).indegree++; }
		void indegree_dec(size_t s) { reg.node_at(s).indegree--; }
		void compute_indegree(const Au* au, state_id_t RootState) {
			this->resize_parallel_hashtab(au->total_states());
			DFS_GraphWalker<state_id_t> dfs;
			dfs.resize(au->total_states());
			dfs.putRoot(RootState);
			while (!dfs.is_finished()) {
				state_id_t i = dfs.next();
				reg.node_at(i).indegree = 0;
				dfs.putChildren(au, i);
			}
			dfs.resize(au->total_states());
			dfs.putRoot(RootState);
			while (!dfs.is_finished()) {
				state_id_t i = dfs.next();
				au->for_each_dest(i, [&](state_id_t t){
					this->reg.node_at(t).indegree++;
				});
				dfs.putChildren(au, i);
			}
		}
		void resize_indegree(size_t n) { this->resize_parallel_hashtab(n); }
	};

	class NestRegisterTable3 {
		Au* au;
	public:
		explicit NestRegisterTable3(Au* a) : au(a) {}
		State& node_at(size_t idx) {
			assert(idx < au->total_states());
			return au->states[idx];
		}
		bool is_deleted(size_t idx) const {
			assert(idx < au->total_states());
			return Au::hash_delmark == au->states[idx].next;
		}
		size_t insert_at(size_t idx) { return au->insert_at(idx); }
		size_t erase_i(size_t idx) { return au->erase_i(idx); }
		size_t size() const { return au->total_states(); }
		void resize(size_t newsize) { au->resize_states(newsize); }
		void erase_all() { au->erase_all(); }
	};
	struct RegisterTable3 {
		NestRegisterTable3 reg;
		explicit RegisterTable3(Au* au) : reg(au) {}
	};

	typedef Base_Parallel_Register<
		typename boost::mpl::if_c
		  < Au::has_register
		  , RegisterTable3
		  , typename boost::mpl::if_c
			< (StateID_Bits > 32 && StateID_Bits % 16 != 0) && 1
			, RegisterTable2<Ordered_Node2>
			, RegisterTable1<Ordered_Node1>
			>::type
		  >::type
		> Ordered_Parallel_Register;

	typedef UnOrdered_Parallel_Register_tpl<Base_Parallel_Register<
		typename boost::mpl::if_c
			< (StateID_Bits > 32 && StateID_Bits % 16 != 0) && 1
			, RegisterTable2<UnOrdered_Node2>
			, RegisterTable1<UnOrdered_Node1>
			>::type
		> > UnOrdered_Parallel_Register;

	template<class StateRegister> class   Ordered_Builder;
	template<class StateRegister> class UnOrdered_Builder;
	typedef   Ordered_Builder<  Ordered_Real_Map_Register>   Ordered_Real_Map_Builder;
	typedef UnOrdered_Builder<UnOrdered_Real_Map_Register> UnOrdered_Real_Map_Builder;
	typedef   Ordered_Builder<  Ordered_Parallel_Register>   Ordered_Parallel_Builder;
	typedef UnOrdered_Builder<UnOrdered_Parallel_Register> UnOrdered_Parallel_Builder;

	template<class Builder>
	static bool
	add_key_val_aux(Builder* builder, valvec<auchar_t>* bufstr16,
					auchar_t delim, fstring key, fstring val) {
		if (delim < 256) {
			check_delim(delim, key);
		}
		ptrdiff_t i, kvlen = key.n + val.n + 1;
		bufstr16->resize_no_init(kvlen);
		auchar_t* dst = bufstr16->data();
		fstring16 word(dst, kvlen);
		for (i = 0; i < key.n; ++i) dst[i] = (byte_t)key.p[i];
		dst[i++] = delim;
		dst += i;
		for (i = 0; i < val.n; ++i) dst[i] = (byte_t)val.p[i];
		return builder->add_word16(word);
	}

	template<class Builder>
	static bool
	add_record_aux(Builder* builder, valvec<auchar_t>* bufstr16,
				 auchar_t delim, const fstrvec& record) {
		if (record.size() < 2) {
			THROW_STD(invalid_argument, "record.size=%zd < 2", record.size());
		}
		if (delim < 256) {
			for(size_t i = 0; i < record.size()-1; ++i)
				check_delim(delim, record[i]);
		}
		size_t i, reclen = record.size() - 1;
		for(i = 0; i < record.size(); ++i) reclen += record.slen(i);
		bufstr16->resize_no_init(reclen+1); // +1 is for trailing delim
		auchar_t* dst = bufstr16->data();
		fstring16 word(dst, reclen);
		for(i = 0; i < record.size(); ++i) {
			fstring str = record[i];
			for(size_t k = 0; k < str.size(); ++k) *dst++ = (byte_t)str[k];
			*dst++ = delim;
		}
		return builder->add_word16(word);
	}

	static void check_delim(auchar_t delim, fstring key) {
		auto beg = (const byte_t*)key.begin();
		auto end = (const byte_t*)key.end();
		auto itr = std::find(beg, end, (byte_t)delim);
		assert(itr == end);
		if (itr != end) {
			THROW_STD(invalid_argument,
				"delim=%d[0x%02X] can't be in key", delim, delim);
		}
	}
};

template<class Au>
template<class StateRegister>
class DFA_OnflyMinimize<Au>::UnOrdered_Builder {

	// indegree covers parallel_hashtab
	void grow_indegree(size_t n_grow) {
		size_t new_used = au->num_used_states() + n_grow;
		size_t new_size = std::max(au->total_states(), new_used);
		R.resize_indegree(new_size);
	}
	void clone_path(ptrdiff_t ConfluenceIndex, ptrdiff_t nPrefix, const auchar_t* str) {
		grow_indegree(nPrefix-ConfluenceIndex+1);
		if (ConfluenceIndex > 1)
			R.erase(path[ConfluenceIndex-1]); // erase for future update
		for (ptrdiff_t j = ConfluenceIndex; j <= nPrefix; ++j) {
			state_id_t curr = au->clone_state(path[j]);
			R.indegree_dec(path[j]); // required!!!
			R.indegree_set(curr, 1);
			au->for_each_dest(curr,
				[this](state_id_t t) {
					this->R.indegree_inc(t);
				});
			// set_move: modify the existed transition:
			state_id_t old = au->set_move(path[j-1], path[j]=curr, str[j-1]);
			assert(nil_state != old); TERARK_UNUSED_VAR(old);
		}
	// path[      nPrefix+1 .. len    ] are newly created states
	// path[ConfluenceIndex .. nPrefix] are the clone and not registered
	// path[ConfluenceIndex-1] was deleted from 'R'
	}
public:
	bool add_record(const fstrvec& record) {
		return add_record_aux(this, &bufstr16, 256, record);
	}
	bool add_record(auchar_t delim, const fstrvec& record) {
		return add_record_aux(this, &bufstr16, delim, record);
	}
	bool add_key_val(fstring key, fstring val) {
		return add_key_val_aux(this, &bufstr16, 256, key, val);
	}
	bool add_key_val(auchar_t delim, fstring key, fstring val) {
		return add_key_val_aux(this, &bufstr16, delim, key, val);
	}
	/// @return false: if str has existed
	///          true: if str not existed and added successfully
    bool add_word(const fstring str) {
		return add_word16(copy_to_bufstr16(str));
	}
    bool add_word16(const fstring16 str) {
		ptrdiff_t len = str.n;
		assert((NULL != str.p && len > 0) || 0 == len);
#ifndef NDEBUG
		if (this->bufstr16.data() != str.p) {
			for (ptrdiff_t i = 0; i < len; ++i) {
				assert(str[i] < Au::sigma);
			}
		}
#endif
		path.resize_no_init(len + 1);
		path.fill((state_id_t)nil_state);
		path[0] = RootState;
        ptrdiff_t  nPrefix; // length of prefix
		for (nPrefix = 0; nPrefix < len; ++nPrefix) {
			state_id_t t = au->state_move(path[nPrefix], str[nPrefix]);
			if (nil_state == t)
				goto CheckConfluence;
			path[nPrefix+1] = t;
		}
		if (au->is_term(path[len]))
			return false; // str has existed in the dawg
		else {} // need to check confluence, fall through
    CheckConfluence:
        for (ptrdiff_t j = 1; j <= nPrefix; ++j) {
			if (R.indegree(path[j]) > 1) {
				// ----- path[j] is a confluence state
				clone_path(j, nPrefix, str.p);
				goto MaybeAddSuffix;
			}
        }
    // Now, there is NO any confluence state in path
    // if nPrefix==len,states[path[nPrefix]] will be set_term_bit
    // if nPrefix< len,states[path[nPrefix]] will be add_move
    // Both need to update    path[nPrefix] in register R
	//	R.resize_parallel_hashtab(au->total_states());
		if (nPrefix)
			R.erase(path[nPrefix]);
    // If has confluence, must not erase and re-insert path[nPrefix]
    // because in this case path[nPrefix] is a clone and doesn't in R
	MaybeAddSuffix:
		grow_indegree(len-nPrefix);
		for (ptrdiff_t j = nPrefix; j < len; ++j) {
			state_id_t t = au->new_state();
			R.indegree_set(t, 1);
			au->add_move_checked(path[j], path[j+1]=t, str[j]);
		}
		au->set_term_bit(path[len]);
		minimize_update_path(str);
		return true;
	}

	bool del_word(const fstring str) {
		return del_word16(copy_to_bufstr16(str));
	}
	bool del_word16(const fstring16 str) {
		ptrdiff_t len = str.n;
		assert((NULL != str.p && len > 0) || 0 == len);
#ifndef NDEBUG
		if (this->bufstr16.data() != str.p) {
			for (ptrdiff_t i = 0; i < len; ++i) {
				assert(str[i] < Au::sigma);
			}
		}
#endif
		path.resize_no_init(len + 1);
		path.fill((state_id_t)nil_state);
		path[0] = RootState;
		for (ptrdiff_t i = 0; i < len; ++i) {
			state_id_t t = au->state_move(path[i], str[i]);
			if (nil_state == t)
				return false; // don't match
			path[i+1] = t;
		}
		if (!au->is_term(path[len]))
			return false; // str didn't exist in the dawg
		ptrdiff_t confluenceIdx = len+1;
		for (ptrdiff_t i = 1; i <= len; ++i) {
			if (R.indegree(path[i]) > 1) {
				clone_path(i, len, str.p);
				confluenceIdx = i;
				goto L1;
			}
		}
		R.erase(path[len]);
L1:
		// now path[confluenceIdx-1, .. end) are not in R
		au->clear_term_bit(path[len]);
		for (ptrdiff_t j = len; j > 0; --j) {
			state_id_t& curr = path[j];
			if (!au->has_children(curr) && !au->is_term(curr)) {
				assert(1 == R.indegree(curr));
				if (j > 1 && j < confluenceIdx)
					R.erase(path[j-1]); // delete parent
				au->del_move(path[j-1], str[j-1]);
				au->del_state(curr);
				R.indegree_set(curr, 0);
			} else {
				std::pair<size_t, bool> ib = R.find_or_add(curr);
				if (!ib.second) { // existed an equivalent state
					state_id_t EqState = ib.first;
					if (EqState == curr) {
						assert(j < confluenceIdx-1);
						break;
					}
					assert(j >= 1);
					if (j > 1)
						R.erase(path[j-1]); // delete parent
					au->set_move(path[j-1], EqState, str[j-1]); // modify parent
					au->del_state(curr);
					R.indegree_set(curr, 0); // reset indegree of deleted state
					R.indegree_inc(curr = EqState);
				}
			}
		}
		return true;
	}
private:
	void clone_path2(size_t ConfluenceIndex, const fstring16 str) {
		if (ConfluenceIndex > 1)
			R.erase(path[ConfluenceIndex-1]); // erase for future update
		grow_indegree(path.size()-ConfluenceIndex);
		for (size_t j = ConfluenceIndex; j < path.size(); ++j) {
			state_id_t curr = au->clone_state(path[j]);
			R.indegree_dec(path[j]); // required!!!
			R.indegree_set(curr, 1);
			au->for_each_dest(curr,
				[this](state_id_t t) {
					this->R.indegree_inc(t);
				});
			// set_move: modify the existed transition:
			if (j <= str.size()) {
				state_id_t old = au->set_move(path[j-1], curr, str[j-1]);
				assert(nil_state != old); TERARK_UNUSED_VAR(old);
			} else {
				assert(!au->more_than_one_child(path[j-1]));
				au->set_single_child(path[j-1], curr);
			}
			path[j]=curr;
		}
	// path[ConfluenceIndex .. path.size) are the clone and not registered
	// path[ConfluenceIndex-1] was deleted from 'R'
	}
	size_t del_by_prefix_start(const fstring16 prefix) {
		path.resize_no_init(prefix.n+1);
		path[0] = RootState;
		for (ptrdiff_t i = 0; i < prefix.n; ++i) {
			state_id_t t = au->state_move(path[i], prefix[i]);
			if (nil_state == t)
				return 0; // don't match
			path[i+1] = t;
		}
		m_suffix.erase_all();
		// no cycle check, if there are cycles, it goes in infinite loop
		for (size_t i = prefix.n; ; ++i) {
			assert(i+1 == path.size());
			state_id_t s = path[i];
			if (au->more_than_one_child(s)) {
			//	throw std::logic_error("del_by_prefix1: suffix is not unique 1");
				return 3;
			}
			state_id_t t = au->get_single_child(s);
			if (au->is_term(s)) {
				if (nil_state == t)
					break; // s is the most final state
				else {
				//	throw std::logic_error("del_by_prefix1: suffix is not unique 2");
					return 4;
				}
			}
			assert(nil_state != t);
			m_suffix.push_back(au->get_single_child_char(s));
			path.push_back(t);
		}
		m_suffix.push_back('\0');
		return 1;
	}
	size_t del_by_prefix_finish(const fstring16 prefix) {
		ptrdiff_t confluenceIdx = path.size();
		for (size_t i = 1; i < path.size(); ++i) {
			if (R.indegree(path[i]) > 1) {
				clone_path2(i, prefix);
				confluenceIdx = i;
				goto L1;
			}
		}
		R.erase(path.back());
L1:
		// now path[confluenceIdx-1, .. end) are not in R
		au->clear_term_bit(path.back());
		for (ptrdiff_t j = path.size()-1; j > 0; --j) {
			state_id_t& curr = path[j];
			if (!au->has_children(curr) && !au->is_term(curr)) {
				assert(1 == R.indegree(curr));
				if (j > 1 && j < confluenceIdx)
					R.erase(path[j-1]); // delete parent
				if (j <= prefix.n)
					au->del_move(path[j-1], prefix[j-1]);
				else
					au->del_all_move(path[j-1]);
				au->del_state(curr);
				R.indegree_set(curr, 0);
			} else {
				std::pair<size_t, bool> ib = R.find_or_add(curr);
				if (!ib.second) { // existed an equivalent state
					state_id_t EqState = ib.first;
					if (EqState == curr) {
						assert(j < confluenceIdx-1);
						break;
					}
					assert(j >= 1);
					if (j > 1)
						R.erase(path[j-1]); // delete parent
					if (j <= prefix.n)
						au->set_move(path[j-1], EqState, prefix[j-1]); // modify parent
					else {
						// path[j-1] must has only 1 child
						assert(!au->more_than_one_child(path[j-1]));
						au->set_single_child(path[j-1], EqState);
					}
					au->del_state(curr);
					R.indegree_set(curr, 0); // reset indegree of deleted state
					R.indegree_inc(curr = EqState);
				}
			}
		}
		return 1;
	}
	valvec<auchar_t> m_suffix;
public:
	size_t del_by_prefix(const fstring prefix) {
		return del_by_prefix16(copy_to_bufstr16(prefix));
	}
	size_t del_by_prefix16(const fstring16 prefix) {
		auto sure = [](const fstring16) { return true; };
		return del_by_prefix(prefix, sure);
	}
	///@param pred bool pred(const fstring suffix);
	///       -# suffix could be modified
	///       -# if pred return true, replace the suffix
	///       -# if pred return false, do nothing
	///@returns
	///  -# 0 no such prefix
	///  -# 1 action successed with pred's return command
	///  -# 2 deletion is canceled by pred returns false
	///  -# 3,4 number of suffixes is more than 1
	///@notes
	///  suffix of prefix must be unique
	template<class Pred>
	size_t del_by_prefix(const fstring prefix, Pred pred) {
		return del_by_prefix16(copy_to_bufstr16(prefix), pred);
	}
	template<class Pred>
	size_t del_by_prefix16(const fstring16 prefix, Pred pred) {
		assert((NULL != prefix.p && prefix.n > 0) || 0 == prefix.n);
#ifndef NDEBUG
		if (this->bufstr16.data() != prefix.p) {
			for (ptrdiff_t i = 0; i < prefix.n; ++i) {
				assert(prefix[i] < Au::sigma);
			}
		}
#endif
		// split this function into *_start and *_finish could instruct
		// the compiler to reduce code size
		size_t ret = del_by_prefix_start(prefix);
		if (1 != ret)
			return ret;
		bool sure = pred(fstring16(m_suffix.data(), m_suffix.size()-1));
		if (!sure)
			return 2; // deletion is canceled
		return del_by_prefix_finish(prefix);
	}

	explicit UnOrdered_Builder(Au* au, state_id_t RootState = initial_state)
	   	: au(au), R(au)
   	{
		reset_root_state(RootState);
	}

	void reset_root_state(state_id_t RootState) {
		assert(au->num_zpath_states() == 0);
		if (au->num_zpath_states()) {
			THROW_STD(invalid_argument, "num_zpath_states()=%zd"
					, au->num_zpath_states());
		}
		R.reg.erase_all();
		this->RootState = RootState;
		assert(RootState < au->total_states());
		if (au->total_states() > 1) {
			R_init(R, au, RootState);
			R.compute_indegree(au, RootState);
		}
		else {
			R.resize_indegree(0);
			R.resize_indegree(1);
		}
	}

	/// keep empty, for consistency with Ordered_Builder
	void close() {}

	const valvec<state_id_t>& get_path() const { return path; }

private:
	template<class Reg>
	static void R_init(Reg& R, const Au* au, state_id_t RootState) {
		R.resize_parallel_hashtab(au->total_states());
		DFS_GraphWalker<state_id_t> dfs;
		dfs.resize(au->total_states());
		dfs.putRoot(RootState);
		while (!dfs.is_finished()) {
			state_id_t curr = dfs.next();
			ASSERT_isNotFree2(au, curr);
			R.find_or_add(curr);
			dfs.putChildren(au, curr);
		}
	}

	struct IndexEntry {
		state_id_t state;
		state_id_t offset;
	};
	struct CompareIndexEntry {
		bool operator()(IndexEntry x, IndexEntry y) const { return x.state < y.state; }
		bool operator()(IndexEntry x, state_id_t y) const { return x.state < y; }
		bool operator()(state_id_t x, IndexEntry y) const { return x < y.state; }
	};
	// inverse NFA of suffix adfa
	struct InverseNFA {
		valvec<IndexEntry>              index;
		valvec<CharTarget<state_id_t> > data;

		IndexEntry* lower_bound(state_id_t state) {
			return std::lower_bound(index.begin(), index.end()-1
									, state, CompareIndexEntry());
		}
	};
	InverseNFA inv_nfa;
	valvec<state_id_t> stack;
	valvec<state_id_t> tmp_idx;
public:
	void add_suffix_adfa(const fstring str, state_id_t root2) {
		add_suffix_adfa16(copy_to_bufstr16(str), root2);
	}
	void add_suffix_adfa16(const fstring16 str, state_id_t root2) {
		assert(str.n >= 1);
		fstring16 key(str.p, str.n-1);
		auchar_t  delim = str.ende(1);
		add_suffix_adfa(key, delim, root2);
	}
	void add_suffix_adfa(const fstring str, auchar_t delim, state_id_t root2) {
		return add_suffix_adfa16(copy_to_bufstr16(str), delim, root2);
	}
	void add_suffix_adfa16(const fstring16 str, auchar_t delim, state_id_t root2) {
		ptrdiff_t len = str.n;
		assert((NULL != str.p && len > 0) || 0 == len);
#ifndef NDEBUG
		if (this->bufstr16.data() != str.p) {
			for (ptrdiff_t i = 0; i < len; ++i) {
				assert(str[i] < Au::sigma);
			}
		}
#endif
		path.resize_no_init(len + 1);
		path.fill((state_id_t)nil_state);
		path[0] = RootState;
        ptrdiff_t  nPrefix; // length of prefix
		for (nPrefix = 0; nPrefix < len; ++nPrefix) {
			state_id_t t = au->state_move(path[nPrefix], str[nPrefix]);
			if (nil_state == t)
				break;
			path[nPrefix+1] = t;
		}
        for (ptrdiff_t j = 1; j <= nPrefix; ++j) {
			if (R.indegree(path[j]) > 1) {
				// ----- path[j] is a confluence state
				clone_path(j, nPrefix, str.p);
				goto MaybeAddSuffix;
			}
        }
    // Now, there is NO any confluence state in path
    // states[path[nPrefix]] will be add_move
    // Must update path[nPrefix] in register R
		if (nPrefix) {
			R.resize_parallel_hashtab(au->total_states());
			R.erase(path[nPrefix]);
		}
    // If has confluence, must not erase and re-insert path[nPrefix]
    // because in this case path[nPrefix] is a clone and doesn't in R
	MaybeAddSuffix:
		grow_indegree(len-nPrefix);
		for (ptrdiff_t j = nPrefix; j < len; ++j) {
			state_id_t t = au->new_state();
			R.indegree_set(t, 1);
			au->add_move_checked(path[j], path[j+1]=t, str[j]);
		}
		append_adfa_topological(len, root2);
		// now path[len+1..end) is in reverse topological order
		//  so path.back() should be root2 equivalent:
		// 1. root2 have an equivalent state on path.back()
		// 2. root2 self is on path.back()
		au->add_move_checked(path[len], path.back(), delim);
		minimize_update_path(str);
	}
private:
	void minimize_update_path(const fstring16 str) {
		for (ptrdiff_t j = str.n; j >= 0; --j) {
			std::pair<size_t, bool> ib = R.find_or_add(path[j]);
			if (!ib.second) { // existed an equivalent state
				state_id_t EqState = ib.first;
				if (EqState == path[j])
					break;
				assert(j >= 1);
				assert(1 == R.indegree(path[j]));
				if (j > 1)
					R.erase(path[j-1]); // delete parent
				au->set_move(path[j-1], EqState, str[j-1]); // modify parent
				au->del_state(path[j]);
				R.indegree_set(path[j], 0); // reset indegree of deleted state
				R.indegree_inc(path[j] = EqState);
			}
		}
	}

	void append_adfa_topological(size_t len, state_id_t root2) {
		assert(stack.empty());
		//
		// append reverse topological order on path
		stack.push_back(root2);
		R.resize_indegree(au->total_states());
	// now indegree serve as 'color' flag for DFS
		while (!stack.empty()) {
			state_id_t parent = stack.back();
			switch (R.indegree(parent)) {
			case 0:
				R.indegree_set(parent, 1);
				au->for_each_dest_rev(parent,
				[&](state_id_t child) {
					switch (R.indegree(child)) {
					case 0:
						stack.push_back(child);
						break;
					case 1: // back edge
						if (child == parent)
							throw std::logic_error("add_suffix_adfa: found a self-loop");
						else
							throw std::logic_error("add_suffix_adfa: found a back-loop");
						break;
					case 2:
						break;
					default:
						assert(0);
						break;
					}
				});
				break;
			case 1:
				R.indegree_set(parent, 2);
			//	printf("topo_path[%zd]=%ld\n", path.size(), (long)parent);
				path.push_back(parent); // on_finish
				// fall through:
				no_break_fallthrough;
			case 2:
				stack.pop_back();
				break;
			default:
				assert(0);
				break;
			}
		}
		assert(path.back() == root2);
		//
		// compute_reverse_adfa_and_indegree:
		//
		// now path[len+1...end) is in reverse topological order
		inv_nfa.index.reserve(path.size()-(len+1)+1);
		inv_nfa.index.erase_all();
		for (size_t j = len+1, n = path.size(); j < n; ++j)
			inv_nfa.index.unchecked_push_back({path[j], 0});
		inv_nfa.index.unchecked_push_back({nil_state+0, 0});
		std::sort(inv_nfa.index.begin(), inv_nfa.index.end()
				, CompareIndexEntry());
		size_t trans_num = 0;
		R.indegree_set(root2, 1);
		for (size_t j = len+1, n = path.size(); j < n; ++j)
			R.indegree_set(path[j], 0);
		for (size_t j = len+1, n = path.size(); j < n; ++j) {
			au->for_each_dest(path[j], [&](state_id_t child) {
				R.indegree_inc(child);
				auto p = inv_nfa.lower_bound(child);
				assert(p < inv_nfa.index.end()-1 && p->state == child);
				p[1].offset++;
				trans_num++;
			});
		}
		tmp_idx.resize_no_init(inv_nfa.index.size());
		tmp_idx[0] = 0;
		for (size_t j = 1; j < inv_nfa.index.size(); ++j)
			tmp_idx[j] = inv_nfa.index[j].offset += inv_nfa.index[j-1].offset;
		inv_nfa.data.resize_no_init(trans_num);
		for (size_t j = len+1, n = path.size(); j < n; ++j) {
			state_id_t parent = path[j];
			au->for_each_move(parent,
			[&,parent](state_id_t child, auchar_t ch) {
				auto p = inv_nfa.lower_bound(child);
				assert(p < inv_nfa.index.end()-1 && p->state == child);
				auto& idx = tmp_idx[p - inv_nfa.index.begin()];
				inv_nfa.data[idx++] = CharTarget<state_id_t>(ch, parent);
			});
		}
		for (size_t j = len+1, n = path.size(); j < n; ++j) {
			state_id_t& child = path[j];
			std::pair<size_t, bool> ib = R.find_or_add(child);
			if (!ib.second) { // existed an equivalent state
				state_id_t EqState = ib.first;
				if (EqState == child)
					break;
				auto p = inv_nfa.lower_bound(child);
				assert(p < inv_nfa.index.end()-1 && p->state == child);
				for (size_t k = p[0].offset; k < p[1].offset; ++k) {
					auto ct = inv_nfa.data[k]; // inverse transition to parent
					au->set_move(ct.target, EqState, ct.ch); // modify parent
				}
				au->del_state(child);
				R.indegree_set(child, 0); // reset indegree of deleted state
				R.indegree_inc(child = EqState);
			}
		}
	}

	fstring16 copy_to_bufstr16(fstring fstr) {
		bufstr16.resize_no_init(fstr.n);
		auchar_t* dst = bufstr16.data();
		for (ptrdiff_t i = 0; i < fstr.n; ++i)
			dst[i] = (byte_t)fstr.p[i];
		return fstring16(dst, fstr.n);
	}

	Au* au;
    valvec<state_id_t> path; // path of traversed states through a string
	valvec<auchar_t> bufstr16;
    StateRegister R; // registered equivalent states
	state_id_t RootState;
};

// nearly 1x faster than UnOrdered_Builder for sorted words
// and less memory usage, 'indegree' is not needed
template<class Au>
template<class StateRegister>
class DFA_OnflyMinimize<Au>::Ordered_Builder {
public:
	explicit Ordered_Builder(Au* au, state_id_t RootState = initial_state)
	  : au(au)
	  , R(au)
   	{
		reset_root_state(RootState);
	}
	void reset_root_state(state_id_t RootState) {
		assert(last_word.empty());
		assert(au->num_zpath_states() == 0);
		if (au->num_zpath_states()) {
			THROW_STD(invalid_argument, "num_zpath_states()=%zd"
					, au->num_zpath_states());
		}
		R.reg.erase_all();
		this->RootState = RootState;
		assert(RootState < au->total_states());
		assert(!au->has_children(RootState));
		R.resize_parallel_hashtab(au->total_states());
	//	R_init(R, au, RootState);
		path.resize_no_init(64); // reserved
		path[0] = RootState;
	}
	~Ordered_Builder() {
		if (!last_word.empty())
			close();
	}

	void close() {
		merge_path(0); // last step of minimization
		last_word.erase_all();
	}

	bool add_key_val(fstring key, fstring val) {
		return add_key_val_aux(this, &bufstr16, 256, key, val);
	}
	bool add_key_val(auchar_t delim, fstring key, fstring val) {
		return add_key_val_aux(this, &bufstr16, delim, key, val);
	}
	///@return true success
	///       false failed: str is already in the automata
	///@throw std::logic_error if str is byte lexical less than last added one
	///@note  str for multiple call of this function must be in byte lex order
    bool add_word(const fstring str) {
		return add_word16(copy_to_bufstr16(str));
	}
    bool add_word16(const fstring16 str) {
		ptrdiff_t len = str.n;
	//	assert((NULL != str.p && len > 0) || 0 == len);
		assert(NULL != str.p || len == 0); // allow empty
		ptrdiff_t max_len = std::max<size_t>(last_word.size(), len);
		ptrdiff_t min_len = std::min<size_t>(last_word.size(), len);
        ptrdiff_t nPrefix; // length of prefix
		const auchar_t* str0 = last_word.data();
		path.resize_no_init(max_len + 1);
		for (nPrefix = 0; nPrefix < min_len; ++nPrefix) {
			if (str.p[nPrefix] != str0[nPrefix]) {
				if (str.p[nPrefix] < str0[nPrefix]) {
				//	assert(0); // raise on debug mode
				//	fprintf(stderr, "last_word=%.*s this_word=%.*s\n", int(last_word.size()), str0, int(len), str);
					throw std::logic_error("1-unordered input to Ordered_Builder::add_word(word)");
				}
				goto OK;
			}
		}
		if (min_len == max_len) // str equal to last_word
			return false;
		else if (min_len == len)
			throw std::logic_error("2-unordered input to Ordered_Builder::add_word(word)");
	OK:
		merge_path(nPrefix); // merge_path after nPrefix for last_word
		size_t required_states = au->num_used_states() + (len-nPrefix);
		if (required_states >= max_state) {
			last_word.resize(0); // avoid errors in merge_path
								 // when stack unwind in this->close()
			string_appender<> msg;
			msg << BOOST_CURRENT_FUNCTION << "required_states=" << required_states
			   	<< ": exceeding max_state=" << max_state;
			throw std::logic_error(msg);
		}
		for (ptrdiff_t i = nPrefix; i < len; ++i) // add suffix
			au->add_move_checked(path[i], path[i+1] = au->new_state(), str[i]);
		au->set_term_bit(path[len]);
		last_word.assign(str.p, str.n);
		return true;
	}
private:
	void merge_path(ptrdiff_t nPrefix) {
		R.resize_parallel_hashtab(au->total_states());
		const auchar_t* str = last_word.data();
		for (ptrdiff_t j = last_word.size(); j > nPrefix; --j) {
			std::pair<size_t, bool> ib = R.find_or_add(path[j]);
			if (!ib.second) {
				state_id_t EqState = ib.first;
				if (EqState == path[j])
					return;
				R.erase(path[j-1]); // delete parent
				au->set_move(path[j-1], EqState, str[j-1]); // modify parent
				au->del_state(path[j]);
				path[j] = EqState;
			}
		}
		assert(!R.is_inserted(path[nPrefix]));
	}

	fstring16 copy_to_bufstr16(fstring fstr) {
		bufstr16.resize_no_init(fstr.n);
		auchar_t* dst = bufstr16.data();
		for (ptrdiff_t i = 0; i < fstr.n; ++i)
			dst[i] = (byte_t)fstr.p[i];
		return fstring16(dst, fstr.n);
	}

	Au* au;
    StateRegister R; // registered equivalent states
    valvec<state_id_t> path; // path of traversed states through a string
	valvec<auchar_t> bufstr16;
	valvec<auchar_t> last_word;
	state_id_t   RootState;
};

template<class DFA>
class MinADFA_OnflyBuilder : public DFA_OnflyMinimize<DFA>::UnOrdered_Parallel_Builder
{
	typedef typename DFA_OnflyMinimize<DFA>::UnOrdered_Parallel_Builder super;
public:
	explicit MinADFA_OnflyBuilder(DFA* au, size_t RootState = initial_state)
	  : super(au, RootState) {}
	typedef typename DFA_OnflyMinimize<DFA>::Ordered_Parallel_Builder Ordered;
};

template<class DFA>
class MinADFA_Module_OnflyBuilder :
	public	DFA_OnflyMinimize<DFA>::UnOrdered_Real_Map_Builder
{
	typedef typename DFA_OnflyMinimize<DFA>::UnOrdered_Real_Map_Builder super;
public:
	explicit MinADFA_Module_OnflyBuilder(DFA* au, size_t RootState = initial_state)
	  : super(au, RootState) {}
	typedef typename DFA_OnflyMinimize<DFA>::Ordered_Real_Map_Builder Ordered;
};

} // namespace terark

#pragma once

#include <boost/type_traits.hpp>
#include <terark/fstring.hpp>
#include <terark/hash_common.hpp>
#include <terark/valvec.hpp>
#include <terark/util/autofree.hpp>
#include <terark/util/throw.hpp>

#include "fsa.hpp"
#include "x_fsa_util.hpp"
#include "dfa_algo_basic.hpp"

namespace terark {

const size_t MAX_ZPATH_LEN_BITS = 8;
const size_t MAX_ZPATH_LEN_MASK = (size_t(1) << MAX_ZPATH_LEN_BITS) - 1;
const size_t MAX_ZPATH_LEN = (size_t(1) << MAX_ZPATH_LEN_BITS) - 2;
const size_t MAX_STATE_BITS = 55; // 64-9

class TERARK_DLL_EXPORT NonRecursiveForEachWord : public ComplexMatchContext {
	struct IncomingEdge {
#if TERARK_WORD_BITS == 64 && 0
	error: LazyUnionDFA::state_id is real 64 bits
		BOOST_STATIC_ASSERT(sizeof(size_t) == 8);
		size_t state : 55;
		size_t label :  9;
#else
		size_t state;
		size_t label;
#endif
		IncomingEdge(size_t state1, auchar_t ch1)
			: state(state1), label(ch1) {}
		void set_visited(size_t zplen) {
			label = 256;
			state = zplen;
		}
		bool is_visited() const { return 256 == label; }
	};

	MatchContext*   m_ctx;
	valvec<byte_t>  m_word_buf;
	valvec<IncomingEdge> m_stack;

	NonRecursiveForEachWord(const NonRecursiveForEachWord&) = delete;
	NonRecursiveForEachWord& operator=(const NonRecursiveForEachWord&) = delete;
	NonRecursiveForEachWord* clone() const override;

	template<class DFA, class OnWord>
	size_t
	run_impl(const DFA& dfa, size_t base_nth, size_t root, size_t SkipLen, OnWord onWord);

public:
	NonRecursiveForEachWord(MatchContext*);
	~NonRecursiveForEachWord();

	static NonRecursiveForEachWord&	cache(MatchContext& ctx) {
		if (ctx.complex_context) {
			assert(dynamic_cast<NonRecursiveForEachWord*>(ctx.complex_context));
			auto p = static_cast<NonRecursiveForEachWord*>(ctx.complex_context);
			assert(&ctx == p->m_ctx);
			return *p;
		} else {
			auto p = new NonRecursiveForEachWord(&ctx);
			ctx.complex_context = p;
			return *p;
		}
	}

	template<class DFA, class OnWord>
	size_t operator()
	(const DFA& dfa, size_t root, size_t SkipLen, OnWord* onWord)
	{return operator()<DFA, OnWord&>(dfa, root, SkipLen, *onWord);}

	template<class DFA, class OnWord>
	size_t operator()
	(const DFA& dfa, size_t base_nth, fstring prefix, size_t root, size_t SkipLen, OnWord* onWord)
	{return operator()<DFA, OnWord&>(dfa, base_nth, prefix, root, SkipLen, *onWord);}

	template<class DFA, class OnWord>
	size_t
	operator()(const DFA& dfa, size_t root, size_t SkipLen, OnWord onWord);

	template<class DFA, class OnWord>
	size_t
	operator()(const DFA& dfa, size_t base_nth, fstring prefix, size_t root, size_t SkipLen, OnWord onWord);
};

template<class DFA, class OnWord>
size_t
NonRecursiveForEachWord::operator()
(const DFA& dfa, size_t root, size_t SkipLen, OnWord onWord)
{
	assert(root < dfa.total_states());
	assert(m_stack.empty());
	m_word_buf.risk_set_size(0);
	return run_impl<DFA, OnWord>(dfa, 0, root, SkipLen, onWord);
}

template<class DFA, class OnWord>
size_t
NonRecursiveForEachWord::operator()
(const DFA& dfa, size_t base_nth, fstring prefix, size_t root, size_t SkipLen, OnWord onWord)
{
	assert(root < dfa.total_states());
	assert(m_stack.empty());
	m_word_buf.assign(prefix.udata(), prefix.size());
	return run_impl<DFA, OnWord>(dfa, base_nth, root, SkipLen, onWord);
}

template<class DFA, class OnWord>
size_t
NonRecursiveForEachWord::run_impl
(const DFA& dfa, size_t base_nth, size_t root, size_t SkipLen, OnWord onWord)
{
	assert(root < dfa.total_states());
	assert(m_stack.empty());
	size_t nth = base_nth;
	auto callbackAndPutChildren = [&](size_t state) {
		if (dfa.is_term(state)) {
			size_t  len = this->m_word_buf.size();
			this->m_word_buf.ensure_capacity(len + 1);
			byte_t* str = this->m_word_buf.data();
			str[len] = '\0'; // use data() to prevent assert
			onWord(nth, fstring(str, len), state);
			nth++;
		}
		size_t oldsize = this->m_stack.size();
		this->m_stack.ensure_capacity(oldsize + DFA::sigma);
		dfa.for_each_move(state, [&](size_t child, auchar_t label) {
			if (DFA::sigma > 256 && label >= 256) {
				// this THROW will be optimized out when DFA::sigma <= 256
				THROW_STD(out_of_range,
					"expect a byte, but got label=%d(0x%X)", label, label);
			}
			this->m_stack.unchecked_emplace_back(child, label);
		});
		std::reverse(this->m_stack.begin() + oldsize, this->m_stack.end());
	};

	if (dfa.num_zpath_states() && dfa.is_pzip(root)) {
		fstring zstr = dfa.get_zpath_data(root, m_ctx);
		assert(SkipLen <= zstr.size());
		m_word_buf.append(zstr.data()+SkipLen, zstr.size()-SkipLen);
	}
	else {
		assert(0 == SkipLen);
		assert(!dfa.is_pzip(root));
	}
	callbackAndPutChildren(root);

	if (dfa.num_zpath_states() == 0) {
		// do not need to access pzip bit, reduced a random memory access
		assert(dfa.total_zpath_len() == 0);
		while (!m_stack.empty()) {
			auto top = m_stack.back();
			size_t state = top.state;
			if (top.is_visited()) {
				size_t zplen = state; // now top.state is zplen
				assert(m_word_buf.size() >= 1 + zplen);
				m_word_buf.pop_n(1 + zplen);
				m_stack.pop_back();
			}
			else {
				assert(state < dfa.total_states());
				assert(!dfa.is_pzip(state));
				m_word_buf.push_back(byte_t(top.label));
				m_stack.back().set_visited(0); // zplen=0
				callbackAndPutChildren(state);
			}
		}
		return nth;
	}
	assert(dfa.total_zpath_len() > 0);
	while (!m_stack.empty()) {
		auto top = m_stack.back();
		size_t state = top.state;
		if (top.is_visited()) {
			size_t zplen = state; // now top.state is zplen
			assert(m_word_buf.size() >= 1 + zplen);
			m_word_buf.pop_n(1 + zplen);
			m_stack.pop_back();
		}
		else {
			assert(state < dfa.total_states());
			m_word_buf.push_back(byte_t(top.label));
			size_t zplen = 0;
			if (dfa.is_pzip(state)) {
				fstring zstr = dfa.get_zpath_data(state, m_ctx);
				zplen = zstr.size();
				m_word_buf.append(zstr.begin(), zplen);
			}
			m_stack.back().set_visited(zplen);
			callbackAndPutChildren(state);
		}
	}
	return nth;
}

// Au should be a reference or a lightweight object (such as a proxy)
template<class Au, class OnNthWord, int NestBufsize = 128>
struct ForEachWord_DFS : private boost::noncopyable {
	BOOST_STATIC_ASSERT(boost::is_reference<Au>::value || sizeof(Au) == sizeof(void*));
	const Au  au;
	char*     beg;
	char*     cur;
	char*     end;
	size_t	  nth;
	MatchContext* ctx;
	OnNthWord on_nth_word;
	char      nest_buf[NestBufsize];

//	typedef typename Au::state_id_t state_id_t;
	typedef size_t state_id_t;
	ForEachWord_DFS(const Au au1, MatchContext* ctx1, OnNthWord op)
	 : au(au1), nth(0), on_nth_word(op) {
		ctx = ctx1;
		beg = nest_buf;
		cur = nest_buf;
		end = nest_buf + NestBufsize;
	}
	template<class... FuncParam>
	ForEachWord_DFS(const Au au1, MatchContext* ctx1, const FuncParam&... param)
	 : au(au1), nth(0), on_nth_word(param...) {
		ctx = ctx1;
		beg = nest_buf;
		cur = nest_buf;
		end = nest_buf + NestBufsize;
	}
	~ForEachWord_DFS() {
		if (nest_buf != beg) {
			assert(NULL != beg);
			free(beg);
		}
	}
	void start(state_id_t RootState, size_t SkipRootPrefixLen) {
		nth = 0;
		cur = beg;
		enter(RootState, SkipRootPrefixLen);
		au.for_each_move(RootState, this);
	}
	void start4(size_t base_nth, fstring prefix, size_t root, size_t zidx) {
		nth = base_nth;
		cur = beg;
		append(prefix.p, prefix.n);
		enter(root, zidx);
		au.for_each_move(root, this);
	}
	size_t enter(state_id_t tt, size_t SkipLen) {
		size_t slen = 0;
		if (au.is_pzip(tt)) {
			fstring zs = au.get_zpath_data(tt, ctx);
			slen = zs.size();
			assert(SkipLen <= slen);
			this->append(zs.data() + SkipLen, slen - SkipLen);
		} else {
			assert(0 == SkipLen);
		}
		if (au.is_term(tt)) {
			this->push_back('\0');
			--cur;
			fstring fstr(beg, cur);
			on_nth_word(nth, fstr, tt);
			++nth;
		}
		return slen;
	}
	// call by Au::for_each_move
	void operator()(state_id_t tt, auchar_t ch) {
		this->push_back((byte_t)ch);
		const size_t slen = enter(tt, 0) + 1;
		au.for_each_move(tt, this);
		assert(cur >= beg + slen);
		cur -= slen;
	}

private:
	void enlarge(size_t inc_size) {
		size_t capacity = end-beg;
		size_t cur_size = cur-beg;
		size_t required_total_size = cur_size + inc_size;
		while (capacity < required_total_size) {
			capacity *= 2;
		}
		char* q;
		if (nest_buf == beg) {
			assert(cur_size <= NestBufsize);
			q = (char*)malloc(capacity);
			if (NULL == q) throw std::bad_alloc();
			memcpy(q, nest_buf, cur_size);
		}
		else {
			q = (char*)realloc(beg, capacity);
			if (NULL == q) throw std::bad_alloc();
		}
		beg = q;
		cur = beg + cur_size;
		end = beg + capacity;
	}
	void push_back(char ch) {
		assert(cur <= end);
		if (cur < end)
			*cur++ = ch;
		else {
		   	enlarge(1);
			*cur++ = ch;
		}
	}
	void append(const void* p, size_t n) {
		if (cur+n > end) {
			enlarge(n);
			assert(cur+n <= end);
		}
		memcpy(cur, p, n);
		cur += n;
	}
};

template<class Au, class OnNthWord, size_t BufferSize>
struct ForEachWord_DFS_FixedBuffer : private boost::noncopyable {
	BOOST_STATIC_ASSERT(boost::is_reference<Au>::value || sizeof(Au) == sizeof(void*));
	const Au  au;
	size_t    cur; // current position to nest_buf
	size_t	  nth;
	MatchContext* ctx;
	OnNthWord on_nth_word;
	char      nest_buf[BufferSize];

//	typedef typename Au::state_id_t state_id_t;
	typedef size_t state_id_t;
	ForEachWord_DFS_FixedBuffer(const Au au1, MatchContext* ctx1, OnNthWord op)
		: au(au1), cur(0), nth(0), ctx(ctx1), on_nth_word(op) { }
	template<class... FuncParam>
	ForEachWord_DFS_FixedBuffer(const Au au1, MatchContext* ctx1, const FuncParam&... param)
		: au(au1), cur(0), nth(0), ctx(ctx1), on_nth_word(param...) {
	}
	void start(state_id_t RootState, size_t SkipRootPrefixLen) {
		nth = 0;
		cur = 0;
		enter(RootState, SkipRootPrefixLen);
		au.for_each_move(RootState, this);
	}
	void start4(size_t base_nth, fstring prefix, size_t root, size_t zidx) {
		nth = base_nth;
		cur = 0;
		append(prefix.p, prefix.n);
		enter(root, zidx);
		au.for_each_move(root, this);
	}
	size_t enter(state_id_t tt, size_t SkipLen) {
		size_t slen = 0;
		if (au.is_pzip(tt)) {
			fstring zs = au.get_zpath_data(tt, ctx);
			slen = zs.size();
			assert(SkipLen <= slen);
			this->append(zs.data() + SkipLen, slen - SkipLen);
		}
		else {
			assert(0 == SkipLen);
		}
		if (au.is_term(tt)) {
			fstring fstr(nest_buf, cur);
			on_nth_word(nth, fstr, tt);
			++nth;
		}
		return slen;
	}
	// call by Au::for_each_move
	void operator()(state_id_t tt, auchar_t ch) {
		this->push_back((byte_t)ch);
		const size_t slen = enter(tt, 0) + 1;
		au.for_each_move(tt, this);
		assert(cur >= slen);
		cur -= slen;
	}

private:
	void push_back(char ch) {
		assert(cur < BufferSize);
		nest_buf[cur++] = ch;
	}
	void append(const void* p, size_t n) {
		assert(cur + n <= BufferSize);
		memcpy(nest_buf + cur, p, n);
		cur += n;
	}
};

template<class OnMatchKeyFunc = const AcyclicPathDFA::OnMatchKey& >
struct ForEachOnMatchKey {
	size_t keylen;
	OnMatchKeyFunc f; // must be compatible with AcyclicPathDFA::OnMatchKey
	ForEachOnMatchKey(size_t klen,OnMatchKeyFunc f1) : keylen(klen),f(f1){}
	void operator()(size_t nth,fstring val,size_t)const{f(keylen,nth,val);}
};
template<class OnNthWordFunc = const AcyclicPathDFA::OnNthWord& >
struct ForEachOnNthWord {
	OnNthWordFunc f; // must be compatible with AcyclicPathDFA::OnNthWord
	ForEachOnNthWord(OnNthWordFunc f1) : f(f1) {}
	void operator()(size_t nth,fstring word,size_t)const{f(nth,word);}
};

template<class DstDFA, class SrcDFA = DstDFA>
struct WorkingCacheOfCopyFrom {
	valvec<CharTarget<size_t> > allmove;
	valvec<typename DstDFA::state_id_t> idmap;
	valvec<typename SrcDFA::state_id_t> stack;

	WorkingCacheOfCopyFrom(size_t SrcTotalStates) {
		idmap.resize_fill(SrcTotalStates, 0+DstDFA::nil_state);
		stack.reserve(512);
	}

	// once constructed, copy_from can be called multiple times for
	// same SrcDFA with different SrcRoot
	// dead states of SrcDFA will not be touched and copied
	typename SrcDFA::state_id_t
	copy_from(DstDFA* au, const SrcDFA& au2, size_t SrcRoot = initial_state) {
		return copy_from(au, au->new_state(), au2, SrcRoot);
	}
	typename SrcDFA::state_id_t
	copy_from(DstDFA* au, size_t DstRoot, const SrcDFA& au2, size_t SrcRoot = initial_state) {
		assert(0 == au->num_zpath_states());
		assert(SrcRoot < au2.total_states());
		assert(idmap.size() == au2.total_states());

		// SrcRoot should not have been touched
		assert(DstDFA::nil_state == idmap[SrcRoot]);
		idmap[SrcRoot] = DstRoot; // idmap also serve as "color"

		stack.erase_all();
		stack.push_back((typename SrcDFA::state_id_t)SrcRoot);
		allmove.reserve(au2.get_sigma());
		while (!stack.empty()) {
			auto parent = stack.back(); stack.pop_back();
			size_t old_stack_size = stack.size();
			assert(!au2.is_pzip(parent));
			assert(DstDFA::nil_state != idmap[parent]);
			allmove.erase_all();
			au2.for_each_move(parent,
				[&](typename SrcDFA::state_id_t child, auchar_t c)
			{
				if (this->idmap[child] == DstDFA::nil_state) {
					this->idmap[child] = au->new_state();
					this->stack.push_back(child);
				}
				else {
					assert(this->idmap[child] < au->total_states());
				}
				this->allmove.emplace_back(c, this->idmap[child]);
			});
			typename DstDFA::state_id_t dst_parent = idmap[parent];
			au->add_all_move(dst_parent, allmove);
			if (au2.is_term(parent))
				au->set_term_bit(dst_parent);
			std::reverse(stack.begin() + old_stack_size, stack.end());
		}
		assert(DstRoot == idmap[SrcRoot]);
		return DstRoot;
	}
};

class DFA_OnWord3 {
public:
	void operator()(size_t nth, fstring w, size_t) const {
		on_word(nth, w);
	}
	const AcyclicPathDFA::OnNthWord& on_word;
	DFA_OnWord3(const AcyclicPathDFA::OnNthWord& on) : on_word(on) {}
};

struct DFA_Key {
	size_t s1; // Small DFA state (Key), not path-zipped
	size_t s2; // Large DFA state (DataBase)
	size_t zidx; // for s2, zidx make the algorithm more complex
	struct HashEqual {
		size_t hash(const DFA_Key& x) const {
			return FaboHashCombine(x.s1, FaboHashCombine(x.s2, x.zidx));
		}
		bool equal(const DFA_Key& x, const DFA_Key& y) const {
			return x.s1 == y.s1 && x.s2 == y.s2 && x.zidx == y.zidx;
		}
	};
//	DFA_Key(size_t s1_, uint32_t s2_, uint32_t zidx_) : s1(s1_), s2(s2_), zidx(zidx_) {}
};

// with depth
struct DFA_Key2 {
	size_t s1; // Small DFA state (Key), not path-zipped
	size_t s2; // Large DFA state (DataBase)
	size_t zidx; // for s2, zidx make the algorithm more complex
	size_t depth;
	struct HashEqual {
		size_t hash(const DFA_Key2& x) const {
			return FaboHashCombine(x.s1, FaboHashCombine(x.s2, x.zidx));
		}
		bool equal(const DFA_Key2& x, const DFA_Key2& y) const {
			return x.s1 == y.s1 && x.s2 == y.s2 && x.zidx == y.zidx;
		}
	};
//	DFA_Key2(size_t s1_, uint32_t s2_, uint32_t zidx_) : s1(s1_), s2(s2_), zidx(zidx_) {}
};

///@returns number of int32 readed
template<class DFA, class Collector>
size_t
dfa_read_binary_int32(const DFA& dfa, size_t state, Collector* vec) {
	auto read_int32 = [vec,state](size_t nth, fstring val, size_t ts) {
		assert(val.size() == sizeof(int32_t));
		int32_t regex_id;
		memcpy(&regex_id, val.data(), sizeof(regex_id));
		if (0) // avoid used param warn
			printf("dfa_read_binary_int32: state %zd, nth %zd, ts %zd, regex_id %d\n", state, nth, ts, regex_id);
		vec->push_back(regex_id);
	};
	BOOST_STATIC_ASSERT(sizeof(size_t) >= 4);
	const int Bufsize = sizeof(size_t); // CPU word size
	ForEachWord_DFS_FixedBuffer<const DFA&, decltype(read_int32), Bufsize> vis(dfa, NULL, read_int32);
#if defined(NDEBUG)
	vis.start(state, 0);
#else
	size_t oldsize = vec->size();
	vis.start(state, 0);
	assert(vis.nth > 0);
	assert(vec->size() == oldsize + vis.nth);
	for (size_t i = oldsize; i < vec->size() - 1; ++i) {
		assert((*vec)[i] != (*vec)[i+1]);
	}
#endif
	return vis.nth;
}

template<class TR, class DFA, class InIter>
size_t
dfa_loop_state_move_aux(const DFA& dfa, size_t state, InIter& beg, InIter end, TR tr) {
	assert(state < dfa.total_states());
	size_t target = state;
	InIter pos = beg;
	for (; pos != end && target == state; ++pos)
		target = dfa.state_move(state, (byte_t)tr(byte_t(*pos)));
	beg = pos;
	return target;
}

template<class TR, class DFA, class InIter>
size_t
dfa_loop_state_move(const DFA& dfa, size_t state, InIter& beg, InIter end, TR tr)
{ return dfa_loop_state_move_aux<TR>(dfa, state, beg, end, tr); }

template<class TR, class DFA, class InIter>
size_t
dfa_loop_state_move(const DFA& dfa, size_t state, InIter& beg, InIter end, TR* tr)
{ return dfa_loop_state_move<TR&>(dfa, state, beg, end, *tr); }

template<class DFA, class InIter>
size_t
dfa_loop_state_move(const DFA& dfa, size_t state, InIter& beg, InIter end)
{ return dfa_loop_state_move(dfa, state, beg, end, IdentityTR()); }


template<class DataIO>
void load_kv_delim_and_is_dag(BaseDFA* dfa, DataIO& dio) {
	uint16_t kv_delim;
	uint08_t b_is_dag;
	dio >> kv_delim;
	dio >> b_is_dag;
	if (kv_delim >= 512) {
		typedef typename DataIO::my_DataFormatException bad_format;
		throw bad_format("kv_delim must < 512");
	}
	dfa->set_kv_delim(kv_delim);
	dfa->set_is_dag(b_is_dag ? 1 : 0);
}

} // namespace terark

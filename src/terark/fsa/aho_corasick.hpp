#pragma once

#include "base_ac.hpp"
#include "automata.hpp"
#include "double_array_trie.hpp"
#include "full_dfa.hpp"

namespace terark {

template<class State>
struct AC_State;

template<>
struct AC_State<State32> : State32 {
    AC_State() {
        output = 0;
        lfail = 0; // initial_state
    }
    uint32_t output; // start index to total output set
    uint32_t lfail;  // link to fail state
	const static uint32_t max_output = max_state;
};
BOOST_STATIC_ASSERT(sizeof(AC_State<State32>) == 16);

#pragma pack(push,1)

#if !defined(_MSC_VER)
	template<>
	struct AC_State<State5B> : State5B {
		AC_State() {
			output = 0;
			lfail = 0; // initial_state
		}
		unsigned output : 30; // start index to total output set
		unsigned lfail  : 26; // link to fail state
		const static uint32_t max_output = max_state;
	};
	BOOST_STATIC_ASSERT(sizeof(AC_State<State5B>) == 12);
#endif

template<>
struct AC_State<State4B> : State4B {
    AC_State() {
        output = 0;
        lfail = 0; // initial_state
    }
	uint32_t output : 15; // start index to total output set
    uint32_t lfail  : 17; // link to fail state
	const static uint32_t max_state = 0x1FFFE; // 128K-2
//	const static uint32_t nil_state = 0x1FFFF; // 128K-1, Don't override nil_state
	const static uint32_t max_output = 0x07FFF; //  32K-1
};
BOOST_STATIC_ASSERT(sizeof(AC_State<State4B>) == 8);

template<>
struct AC_State<DA_State8B> : DA_State8B {
    AC_State() {
        output = 0;
        lfail = 0; // initial_state
    }
    uint32_t output; // start index to total output set
    uint32_t lfail;  // link to fail state
	const static uint32_t max_output = max_state;
};
BOOST_STATIC_ASSERT(sizeof(AC_State<DA_State8B>) == 16);

#pragma pack(pop)

// to be customized at call side
inline bool adl_ac_scan_should_prefetch(...) { return false; }

// Aho-Corasick implementation
template<class BaseAutomata>
class Aho_Corasick : public BaseAutomata, public BaseAC {
    typedef BaseAutomata super;
public:
    typedef typename BaseAutomata::state_t    state_t;
    typedef typename BaseAutomata::state_id_t state_id_t;
    using super::states;
    using super::nil_state;
private:
//	word_id_t is typedef'ed in BaseAC
    typedef valvec<word_id_t> output_t;
	output_t output; // compacted output

    void set_fail(state_id_t s, state_id_t f) { states[s].lfail = f; }
    state_id_t  ffail(state_id_t s) const { return states[s].lfail; }

    class BFS_AC_Builder; friend class BFS_AC_Builder;
    class BFS_AC_Counter; friend class BFS_AC_Counter;
public:
	const static word_id_t null_word = ~word_id_t(0);
	class ac_builder; friend class ac_builder;

	~Aho_Corasick() {
		if (this->mmap_base) {
			output.risk_release_ownership();
			offsets.risk_release_ownership();
			strpool.risk_release_ownership();
		}
	}

	const BaseAC* get_ac() const override final { return this; }

	bool full_dfa_is_term(size_t state) const {
		assert(state < states.size() - 1);
		size_t off0 = states[state + 0].output;
		size_t off1 = states[state + 1].output;
		assert(off0 <= off1);
		return off0 < off1;
	}

	size_t full_dfa_state_move(size_t state, auchar_t ch) const {
		assert(state < states.size() - 1);
		size_t child;
		size_t back = state;
		while ((child = this->state_move(back, ch)) == nil_state) {
			if (initial_state == back)
				return initial_state;
			back = this->ffail(back);
			assert(back < states.size() - 1);
		}
		return child;
	}

	template<class OP>
	void full_dfa_for_each_move(size_t state, OP op) const {
		for (auchar_t ch = 0; ch < 256; ++ch) {
			size_t child = full_dfa_state_move(state, ch);
			assert(child != nil_state);
			op(child, ch);
		}
	}

	template<class OP>
	void full_dfa_for_each_dest(size_t state, OP op) const {
		for (auchar_t ch = 0; ch < 256; ++ch) {
			size_t child = full_dfa_state_move(state, ch);
			assert(child != nil_state);
			op(child);
		}
	}

	template<class OP>
	void full_dfa_for_each_dest_rev(size_t state, OP op) const {
		for (auchar_t ch = 255; ch > 0; ) {
			size_t child = full_dfa_state_move(state, --ch);
			assert(child != nil_state);
			op(child);
		}
	}

	size_t full_dfa_read_matchid(size_t state, valvec<int>* vec) const {
		assert(state < states.size() - 1);
		size_t off0 = states[state + 0].output;
		size_t off1 = states[state + 1].output;
		assert(off0 < off1);
		vec->append(output.data() + off0, off1 - off0);
		return off1 - off0;
	}

	size_t mem_size() const override {
        return super::mem_size() + sizeof(output[0]) * output.size();
    }
	void clear() {
		output.clear();
		super::clear();
	}
	void swap(Aho_Corasick& y) {
		assert(typeid(*this) == typeid(y));
		super::risk_swap(y);
		output.swap(y.output);
		BaseAC::swap(y);
	}
	using super::total_states;
	using super::total_transitions;
	using super::internal_get_states;
	using super::for_each_move;
//	using super::TreeWalker;

    const valvec<uint32_t>& get_offsets() const { return this->offsets; }
    const valvec<char>&     get_strpool() const { return this->strpool; }
    valvec<uint32_t>&   mutable_offsets()       { return this->offsets; }
    valvec<char>&       mutable_strpool()       { return this->strpool; }

	void ac_scan(fstring input, const OnHitWords& on_hit)
	const override final {
		tpl_ac_scan(input, on_hit);
	}
	void ac_scan(fstring input, const OnHitWords& on_hit, const ByteTR& tr)
	const override final {
		tpl_ac_scan(input, on_hit, tr);
	}
	void ac_scan(ByteInputRange& r, const OnHitWords& on_hit)
	const override final {
		scan_stream(&r, &on_hit);
	}
	void ac_scan(ByteInputRange& r, const OnHitWords& on_hit, const ByteTR& tr)
	const override final {
		scan_stream(&r, &on_hit, &tr);
	}

	const word_id_t* words(size_t state, size_t* num_words) const {
		assert(state < states.size()-1);
		size_t off0 = states[state+0].output;
		size_t off1 = states[state+1].output;
		*num_words = off1 - off0;
		return output.data() + off0;
	}

	template<class Offset>
	word_id_t find(const fstring str) const {
		assert(this->offsets.size() == this->states.size());
		return find(str, this->offsets.data());
	}
	template<class Offset>
	word_id_t find(const fstring str, const Offset* offsets) const {
		state_id_t curr = initial_state;
		for (size_t i = 0; i < str.size(); ++i) {
			state_id_t next = super::state_move(curr, (byte_t)str.p[i]);
			if (nil_state == next)
				return null_word;
			curr = next;
		}
		if (states[curr].is_term()) {
			size_t out0 = states[curr+0].output;
			size_t out1 = states[curr+1].output;
			assert(out1 > out0);
			size_t i = out0;
			do {
				word_id_t word_id = output[i];
				Offset off0 = offsets[word_id+0];
				Offset off1 = offsets[word_id+1];
				if (off1 - off0 == str.size())
					return word_id;
			} while (++i < out1);
		}
		return null_word;
	}

    // res[i].first : KeyWord id
    // res[i].second: end position of str
	// Aho-Corasick don't save strlen(word_id)
	template<class Pos>
	terark_no_alias
    void tpl_ac_scan(fstring str, valvec<std::pair<word_id_t, Pos> >* res) const {
		assert(!output.empty());
		tpl_ac_scan(str, [=](size_t endpos, const word_id_t* hits, size_t n, size_t) {
						for (size_t i = 0; i < n; ++i) {
							std::pair<word_id_t, Pos> x;
							x.first = hits[i];
							x.second = Pos(endpos);
							res->push_back(x);
						}
					});
	}

	template<class OnHitWord>
	terark_no_alias
    void tpl_ac_scan(fstring str, OnHitWord on_hit_word) const {
		tpl_ac_scan<OnHitWord, IdentityTR>(str, on_hit_word, IdentityTR());
	}
	template<class OnHitWord, class TR>
	terark_no_alias
	void tpl_ac_scan(const fstring str, OnHitWord on_hit_word, TR tr) const {
		tpl_ac_scan_aux<OnHitWord, TR>(str, on_hit_word, tr, states.data());
	}

	template<class OnHitWord>
	terark_no_alias
	void tpl_ac_scan(fstring str, size_t beg, size_t len, OnHitWord on_hit_word) const {
		TERARK_ASSERT_LE(beg, str.size());
		TERARK_ASSERT_LE(len, str.size());
		TERARK_ASSERT_LE(beg + len, str.size());
		tpl_ac_scan<OnHitWord, IdentityTR>(str, beg, len, on_hit_word, IdentityTR());
	}
	template<class OnHitWord, class TR>
	terark_no_alias
	void tpl_ac_scan(fstring str, size_t beg, size_t len, OnHitWord on_hit_word, TR tr) const {
		TERARK_ASSERT_LE(beg, str.size());
		TERARK_ASSERT_LE(len, str.size());
		TERARK_ASSERT_LE(beg + len, str.size());
		tpl_ac_scan_aux<OnHitWord, TR>(str.p, beg, len, on_hit_word, tr, states.data());
	}

	// specialized for DoubleArray, this function is extremely fast
	// 720MB/s on i7-4790, msvc2013 x86_32bit, 1000 patterns
	template<class OnHitWord, class TR>
	inline
	terark_no_alias
	void tpl_ac_scan_aux(const fstring str, OnHitWord on_hit_word, TR tr,
						 const AC_State<DA_State8B>* lstates)
	const {
		tpl_ac_scan_aux<OnHitWord, TR>(str.p, 0, str.n, on_hit_word, tr, lstates);
	}
	template<class OnHitWord, class TR>
	inline
	terark_no_alias
	void tpl_ac_scan_aux(const char* str_p, size_t beg, size_t len,
						 OnHitWord on_hit_word, TR tr,
						 const AC_State<DA_State8B>* lstates)
	const {
		assert(!output.empty());
		size_t curr = initial_state;
		auto loutput = output.data();
		size_t endpos = beg + len;
		for(size_t pos = beg; pos < endpos;) {
			size_t c = (byte_t)tr((byte_t)str_p[pos++]);
			size_t next;
			while (lstates[next = lstates[curr].child0() + c].parent() != curr) {
				if (initial_state == curr)
					goto ThisCharEnd;
				curr = lstates[curr].lfail;
				assert(curr < states.size()-1);
			}
			assert(next != initial_state);
			assert(next < states.size()-1);
			curr = next;
			{
				size_t oBeg = lstates[curr + 0].output;
				size_t oEnd = lstates[curr + 1].output;
				assert(oBeg <= oEnd);
				assert(oEnd <= output.size());
				if (oBeg < oEnd) {
					// to be customized at call side, should be a real constexpr
					if (adl_ac_scan_should_prefetch(&on_hit_word)) {
						size_t xc = (byte_t)str_p[pos];
						_mm_prefetch((const char*)(&lstates[lstates[curr].child0() + xc]), _MM_HINT_T0);
					}
					size_t end_pos_of_hit = pos;
					on_hit_word(end_pos_of_hit, loutput + oBeg, oEnd - oBeg, curr);
				}
			}
		ThisCharEnd:;
		}
	}

	// fall back to non-DoubleArray Trie based AC automata
	template<class OnHitWord, class TR>
	inline
	terark_no_alias
	void tpl_ac_scan_aux(const fstring str, OnHitWord on_hit_word, TR tr, ...)
	const {
		assert(!output.empty());
		size_t curr = initial_state;
		for(size_t pos = 0; pos < size_t(str.n);) {
			byte_t c = (byte_t)tr((byte_t)str.p[pos++]);
			size_t next;
			while ((next = this->state_move(curr, c)) == nil_state) {
				if (initial_state == curr)
					goto ThisCharEnd;
				curr = ffail(curr);
				assert(curr < states.size()-1);
			}
			assert(next != initial_state);
			assert(next < states.size()-1);
			curr = next;
			{
				size_t oBeg = states[curr + 0].output;
				size_t oEnd = states[curr + 1].output;
				assert(oBeg <= oEnd);
				assert(oEnd <= output.size());
				if (oBeg < oEnd) {
					size_t end_pos_of_hit = pos;
					on_hit_word(end_pos_of_hit, output.data() + oBeg, oEnd - oBeg, curr);
				}
			}
		ThisCharEnd:;
		}
	}

    // res[i].first : KeyWord id
    // res[i].second: end position of str
	// Aho-Corasick don't save strlen(word_id)
	template<class Pos>
	terark_no_alias
    void tpl_ac_scan_rev(fstring str, valvec<std::pair<word_id_t, Pos> >* res) const {
		assert(!output.empty());
		tpl_ac_scan_rev(str, [=](size_t begpos, const word_id_t* hits, size_t n, size_t) {
						for (size_t i = 0; i < n; ++i) {
							std::pair<word_id_t, Pos> x;
							x.first = hits[i];
							x.second = Pos(begpos);
							res->push_back(x);
						}
					});
	}

	template<class OnHitWord>
	terark_no_alias
    void tpl_ac_scan_rev(fstring str, OnHitWord on_hit_word) const {
		tpl_ac_scan_rev<OnHitWord, IdentityTR>(str, on_hit_word, IdentityTR());
	}
	template<class OnHitWord, class TR>
	terark_no_alias
	void tpl_ac_scan_rev(const fstring str, OnHitWord on_hit_word, TR tr) const {
		tpl_ac_scan_rev_aux<OnHitWord, TR>(str, on_hit_word, tr, states.data());
	}

	// specialized for DoubleArray, this function is extremely fast
	// 720MB/s on i7-4790, msvc2013 x86_32bit, 1000 patterns
	template<class OnHitWord, class TR>
	inline
	terark_no_alias
	void tpl_ac_scan_rev_aux(const fstring str, OnHitWord on_hit_word, TR tr,
						 const AC_State<DA_State8B>* lstates)
	const {
		assert(!output.empty());
		size_t curr = initial_state;
		auto loutput = output.data();
		for(size_t pos = size_t(str.n); pos; ) {
			size_t c = (byte_t)tr((byte_t)str.p[--pos]);
			size_t next;
			while (lstates[next = lstates[curr].child0() + c].parent() != curr) {
				if (initial_state == curr)
					goto ThisCharEnd;
				curr = lstates[curr].lfail;
				assert(curr < states.size()-1);
			}
			assert(next != initial_state);
			assert(next < states.size()-1);
			curr = next;
			{
				size_t oBeg = lstates[curr + 0].output;
				size_t oEnd = lstates[curr + 1].output;
				assert(oBeg <= oEnd);
				assert(oEnd <= output.size());
				if (oBeg < oEnd) {
					on_hit_word(pos, loutput + oBeg, oEnd - oBeg, curr);
				}
			}
		ThisCharEnd:;
		}
	}

	// fall back to non-DoubleArray Trie based AC automata
	template<class OnHitWord, class TR>
	inline
	terark_no_alias
	void tpl_ac_scan_rev_aux(const fstring str, OnHitWord on_hit_word, TR tr, ...)
	const {
		assert(!output.empty());
		size_t curr = initial_state;
		for(size_t pos = size_t(str.n); pos; ) {
			byte_t c = (byte_t)tr((byte_t)str.p[--pos]);
			size_t next;
			while ((next = this->state_move(curr, c)) == nil_state) {
				if (initial_state == curr)
					goto ThisCharEnd;
				curr = ffail(curr);
				assert(curr < states.size()-1);
			}
			assert(next != initial_state);
			assert(next < states.size()-1);
			curr = next;
			{
				size_t oBeg = states[curr + 0].output;
				size_t oEnd = states[curr + 1].output;
				assert(oBeg <= oEnd);
				assert(oEnd <= output.size());
				if (oBeg < oEnd) {
					on_hit_word(pos, output.data() + oBeg, oEnd - oBeg, curr);
				}
			}
		ThisCharEnd:;
		}
	}

	///@param on_hit_word(size_t end_pos_of_hit, const word_id_t* first, size_t num)
	template<class InputStream, class OnHitWord>
    void scan_stream(InputStream* input, OnHitWord* on_hit_word) const {
		scan_stream<InputStream&, OnHitWord&>(*input, *on_hit_word, IdentityTR());
	}

	///@param on_hit_word(size_t end_pos_of_hit, const word_id_t* first, size_t num)
	template<class InputStream, class OnHitWord, class TR>
    void scan_stream(InputStream* input, OnHitWord* on_hit_word, TR* tr) const {
		scan_stream<InputStream&, OnHitWord&, TR&>(*input, *on_hit_word, *tr);
	}

	template<class InputStream, class OnHitWord>
    void scan_stream(InputStream input, OnHitWord on_hit_word) const {
		scan_stream<InputStream, OnHitWord>(input, on_hit_word, IdentityTR());
	}
	template<class InputStream, class OnHitWord, class TR>
    void scan_stream(InputStream input, OnHitWord on_hit_word, TR tr) const {
		assert(!output.empty());
        state_id_t curr = initial_state;
		for (size_t pos = 0; !input.empty();) {
            byte_t c = tr(*input); ++input;
            pos++;
            state_id_t next;
            while ((next = this->state_move(curr, c)) == nil_state) {
                if (initial_state == curr)
                    goto ThisCharEnd;
                curr = ffail(curr);
                assert(curr < (state_id_t)states.size());
            }
            assert(next != initial_state);
            assert(next < (state_id_t)states.size());
            curr = next;
			{
				size_t oBeg = states[curr+0].output;
				size_t oEnd = states[curr+1].output;
				assert(oBeg <= oEnd);
				assert(oEnd <= output.size());
				if (oBeg < oEnd) {
					size_t end_pos_of_hit = pos;
					on_hit_word(end_pos_of_hit, output.data() + oBeg, oEnd-oBeg, curr);
				}
			}
        ThisCharEnd:;
        }
    }

private:
	template<class DoubleArrayAC, class Y_AC>
	friend void double_array_ac_build_from_au(DoubleArrayAC& x_ac, const Y_AC& y_ac, const char* BFS_or_DFS);

	template<class DataIO>
	friend void DataIO_loadObject(DataIO& dio, Aho_Corasick& x) {
		typename DataIO::my_var_size_t n_words;
		dio >> static_cast<super&>(x);
		dio >> x.output;
		dio >> n_words; x.n_words = n_words;
		byte_t word_ext;
		dio >> word_ext;
		if (word_ext >= 1) dio >> x.offsets;
		if (word_ext >= 2) dio >> x.strpool;
	}
	template<class DataIO>
	friend void DataIO_saveObject(DataIO& dio, const Aho_Corasick& x) {
		dio << static_cast<const super&>(x);
		dio << x.output;
		dio << typename DataIO::my_var_size_t(x.n_words);
		byte_t word_ext = 0;
		if (x.has_word_length()) word_ext = 1;
		if (x.has_word_content()) word_ext = 2;
		dio << word_ext;
		if (word_ext >= 1) dio << x.offsets;
		if (word_ext >= 2) dio << x.strpool;
	}

	void finish_load_mmap(const DFA_MmapHeader* base) override {
		byte_t* bbase = (byte_t*)base;
		super::finish_load_mmap(base);
		assert(base->num_blocks >= 2 + base->ac_word_ext);
		assert(base->ac_word_ext <= 2);
		auto   ac_blocks = &base->blocks[base->num_blocks - 1 - base->ac_word_ext];
		size_t num_output = size_t(ac_blocks[0].length / sizeof(word_id_t));
		output.clear();
		output.risk_set_data((word_id_t*)(bbase + ac_blocks[0].offset));
		output.risk_set_size(num_output);
		output.risk_set_capacity(num_output);
		BaseAC::ac_str_finish_load_mmap(base);
	}

	long prepare_save_mmap(DFA_MmapHeader* base, const void** dataPtrs)
	const override final {
		super::prepare_save_mmap(base, dataPtrs);
		auto blocks_end = &base->blocks[base->num_blocks];
		blocks_end[0].offset = align_to_64(blocks_end[-1].endpos());
		blocks_end[0].length = sizeof(word_id_t)* output.size();
		dataPtrs[base->num_blocks++] = output.data();
		BaseAC::ac_str_prepare_save_mmap(base, dataPtrs);
		return 0;
	}

	typedef Aho_Corasick MyType;
public:
#include "ppi/tpl_for_each_word.hpp"
};

/// @param x_ac double array ac
template<class DoubleArrayAC, class Y_AC>
void double_array_ac_build_from_au(DoubleArrayAC& x_ac, const Y_AC& y_ac, const char* BFS_or_DFS) {
	typedef DoubleArrayAC X_AC;
	valvec<typename X_AC::state_id_t> map_y2x;
	valvec<typename Y_AC::state_id_t> map_x2y;
	const size_t extra = 1;
	x_ac.build_from(y_ac, map_x2y, map_y2x, BFS_or_DFS, extra);
	assert(x_ac.states.back().is_free());
	assert(x_ac.output.size() == 0);
	x_ac.output.reserve(y_ac.output.size());
	x_ac.states[0].output = 0;
	for (size_t x = 0; x < map_x2y.size()-extra; ++x) {
		typename Y_AC::state_id_t y = map_x2y[x];
		if (Y_AC::nil_state != y) {
			typename Y_AC::state_id_t f = y_ac.states[y].lfail;
			x_ac.states[x].lfail = map_y2x[f];
			size_t out0 = y_ac.states[y+0].output;
			size_t out1 = y_ac.states[y+1].output;
			x_ac.output.append(y_ac.output.data()+out0, y_ac.output.data()+out1);
		}
		x_ac.states[x+1].output = x_ac.output.size();
	}
	assert(x_ac.output.size() == y_ac.output.size());
	x_ac.n_words = y_ac.n_words;
	x_ac.offsets = y_ac.offsets;
	x_ac.strpool = y_ac.strpool;
}

template<class BaseAutomata>
class Aho_Corasick<BaseAutomata>::BFS_AC_Counter : boost::noncopyable {
	Aho_Corasick*      m_ac;
	state_id_t*        m_pcnt;
	valvec<state_id_t> m_fail;

public:
	BFS_AC_Counter(Aho_Corasick* ac, state_id_t* pcnt) {
		m_ac = ac;
		size_t ssize = ac->total_states() - 1;
		m_fail.resize(ssize, nil_state+0);
		for(size_t i = 0; i < ssize; ++i)
			pcnt[i] = (!ac->is_free(i) && ac->is_term(i)) ? 1 : 0;
		m_fail[initial_state] = initial_state;
		m_pcnt = pcnt;
	}
	void operator()(state_id_t parent, state_id_t curr, auchar_t c) {
		assert(c < 256);
		Aho_Corasick* ac = m_ac;
		state_id_t back = m_fail[parent];
		state_id_t next;
		while ((next = ac->state_move(back, c)) == nil_state) {
			if (initial_state == back) {
				m_fail[curr] = initial_state;
				return;
			}
			back = m_fail[back];
		}
		if (curr != next) {
			m_pcnt[curr] += m_pcnt[next];
			m_fail[curr] = next;
		} else
			m_fail[curr] = initial_state;
	}
};

template<class BaseAutomata>
class Aho_Corasick<BaseAutomata>::BFS_AC_Builder : boost::noncopyable {
public:
	Aho_Corasick* m_ac;
	state_id_t*   m_cursor;
	 word_id_t*   m_output;

	void operator()(state_id_t parent, state_id_t curr, auchar_t c)
	const {
		assert(c < 256);
		Aho_Corasick* ac = m_ac;
		state_id_t back = ac->ffail(parent);
		state_id_t next;
		while ((next = ac->state_move(back, c)) == nil_state) {
			if (initial_state == back) {
				ac->set_fail(curr, initial_state);
				return;
			}
			back = ac->ffail(back);
		}
		if (curr != next) {
			state_t* states = ac->states.data();
			size_t beg2 = states[next + 0].output;
			size_t end2 = states[next + 1].output;
			if (beg2 < end2) {
			// remove set_term_bit, make AC trie be compatible with BaseDFA
			//	states[curr].set_term_bit();
				word_id_t* output = m_output;
				size_t beg1 = states[curr+0].output + m_cursor[curr];
			#if !defined(NDEBUG)
				size_t end1 = states[curr+1].output;
				assert(end1 - beg1 >= end2 - beg2);
			#endif
				m_cursor[curr] += end2 - beg2;
				size_t i = beg1, j = beg2;
				while (j < end2)
					output[i++] = output[j++];
			}
			ac->set_fail(curr, next);
		} else
			ac->set_fail(curr, initial_state);
	}
};

template<class BaseAutomata>
class Aho_Corasick<BaseAutomata>::ac_builder : boost::noncopyable {
	Aho_Corasick* ac;
public:
	static const state_id_t max_output = state_t::max_output;
	size_t all_words_size; // for easy access

	explicit ac_builder(Aho_Corasick* ac) { reset(ac); }

	void reset(Aho_Corasick* ac) {
		assert(ac->total_states() == 1);
		this->ac = ac;
		all_words_size = 0;
		ac->n_words = 0;
		ac->output.resize(1); // keep parallel with states
	}

	///@returns .first  == word_id
	///         .second == true  : successful added the new word
	///         .second == false : has duplicate, word_id indicate
	///                            the existing word_id
	std::pair<word_id_t, bool>
	add_word(const fstring key) {
		assert(key.n > 0); // don't allow empty key
		state_id_t curr = initial_state;
		size_t j = 0;
		while (j < key.size()) {
			state_id_t next = ac->state_move(curr, (byte_t)key.p[j]);
			if (nil_state == next)
				goto AddNewStates;
			assert(next < (state_id_t)ac->states.size());
			curr = next;
			++j;
		}
		// key could be the prefix of some existed key word
		if (ac->states[curr].is_term()) {
			return std::make_pair(ac->output[curr], false);
		} else {
			assert(ac->output[curr] == null_word);
			goto DoAdd;
		}
	AddNewStates:
		do {
			state_id_t next = ac->new_state();
			ac->add_move_checked(curr, next, (byte_t)key.p[j]);
			curr = next;
			++j;
		} while (j < key.size());
		// now curr == next, and it is a Terminal State
		assert(ac->output.size() < ac->states.size());
		ac->output.reserve(ac->states.capacity());
		ac->output.resize(ac->states.size(), (word_id_t)null_word);
	DoAdd:
		if (ac->n_words == max_output) {
			string_appender<> msg;
			msg << "num_words exceeding max_output: " << BOOST_CURRENT_FUNCTION;
			throw std::out_of_range(msg);
		}
		word_id_t word_id = ac->n_words;
		ac->output[curr] = word_id;
		ac->states[curr].set_term_bit();
		all_words_size += key.size();
		ac->n_words += 1;
		return std::make_pair(word_id, true);
	}

	void compile() {
		ac->output.shrink_to_fit();
		ac->states.push_back(); // for guard 'output'
		ac->shrink_to_fit();
		size_t ssize = ac->total_states() - 1;
		valvec<state_id_t> cursor(ssize, valvec_no_init());
		{ // use cursor as counter
			BFS_AC_Counter cnt(ac, cursor.data());
			ac->bfs_tree(state_id_t(initial_state), &cnt);
		}
		state_t* states = ac->states.data();
		size_t total = 0;
		for(size_t i = 0; i < ssize; ++i) {
			states[i].output = total;
			total += cursor[i];
		}
		if (total >= max_output) {
			string_appender<> msg;
			msg << "num_words exceed max_output: " << BOOST_CURRENT_FUNCTION;
			throw std::out_of_range(msg);
		}
		states[ssize].output = total; // the guard

		{ // init ac->output for propagation
			valvec<word_id_t> owords(total, valvec_no_init());
			owords.swap(ac->output);
			word_id_t* output = ac->output.data();
			for(size_t i = 0; i < ssize; ++i) {
				state_t& si = states[i];
				if (!si.is_free() && si.is_term()) {
					output[si.output] = owords[i];
					cursor[i] = 1;
				} else
					cursor[i] = 0;
			}
		}
		BFS_AC_Builder build;
		build.m_ac = ac;
		build.m_cursor = cursor.data();
		build.m_output = ac->output.data();
		ac->set_fail(initial_state, initial_state);
		ac->bfs_tree(state_id_t(initial_state), &build);
	#if !defined(NDEBUG)
		for(size_t i = 0; i < ssize; ++i) {
			size_t cnt = states[i+1].output - states[i].output;
			assert(cnt == cursor[i]);
		}
	#endif
	}

	void lex_ordinal_fill_word_offsets() {
		assert(ac->offsets.size() == 0);
		set_word_id_as_lex_ordinal(&ac->offsets, NULL);
	}
	void lex_ordinal_fill_word_contents() {
		assert(ac->offsets.size() == 0);
		set_word_id_as_lex_ordinal(&ac->offsets, &ac->strpool);
	}

	void set_word_id_as_lex_ordinal() {
		assert(ac->offsets.size() == 0);
		word_id_t* owords = ac->output.data();
		ac->for_each_word(
			[&](size_t nth, fstring, state_id_t s) {
				owords[s] = nth; // overwrite old_word_id
			});
	}

	/// When calling add_word, pass word_id as any thing, then call this function
	/// then call compile:
	///@code
	///   AC_T::ac_builder builder(&ac);
	///   builder.add_word("abc");
	///   ...
	///   builder.set_word_id_as_lex_ordinal(&offsets, &strpool);
	///   builder.compile();
	///   ac.tpl_ac_scan(...);
	///@endcode
	template<class Offset>
	void set_word_id_as_lex_ordinal(valvec<Offset>* offsets, valvec<char>* strpool) {
		if (strpool)
			strpool->resize(0);
		offsets->resize(0);
		offsets->reserve(ac->n_words+1);
		if (strpool)
			strpool->reserve(all_words_size);
		Offset curr_offset = 0;
		word_id_t* owords = ac->output.data();
		ac->tpl_for_each_word(
			[&](size_t nth, fstring word, state_id_t s) {
				owords[s] = nth; // overwrite old_word_id
				offsets->unchecked_push_back(curr_offset);
				if (strpool)
					strpool->append(word);
				curr_offset += word.size();
			});
		offsets->unchecked_push_back(curr_offset);
		assert(offsets->size() == ac->n_words+1);
		assert(curr_offset == all_words_size);
	}

	template<class StrVec>
	void set_word_id_as_lex_ordinal(StrVec* vec) {
		vec->resize(0);
		vec->reserve(ac->n_words);
		word_id_t* owords = ac->output.data();
		ac->tpl_for_each_word(
			[&](size_t nth, fstring word, state_id_t s) {
				owords[s] = nth; // overwrite old_word_id
				vec->push_back(word);
			});
		vec->shrink_to_fit();
		assert(vec->size() == ac->n_words);
	}

	template<class WordCollector>
	void set_word_id_as_lex_ordinal(WordCollector word_collector) {
		word_id_t* owords = ac->output.data();
		ac->tpl_for_each_word(
			[=](size_t nth, fstring word, state_id_t s) {
				size_t old_word_id = owords[s];
				owords[s] = nth; // nth is new word_id
				word_collector(nth, old_word_id, word);
			});
	}

	void sort_by_word_len() {
		sort_by_word_len(ac->offsets);
	}

	/// @note: before calling this function:
	///   -# word_id must be lex_ordinal
	///   -# WordAttribute array contains the offsets as set_word_id_as_lex_ordinal
	template<class Offset>
	void sort_by_word_len(const valvec<Offset>& offsets) {
		sort_by_word_len(offsets.begin(), offsets.end());
	}
	template<class Offset>
	void sort_by_word_len(const Offset* beg, const Offset* end) {
		sort_by_word_len(beg, end, [](Offset off){return off;});
	}
	// Offset could be a member of WordAttributs, it is extracted by get_offset
	template<class WordAttributs, class GetOffset>
	void sort_by_word_len(const valvec<WordAttributs>& word_attrib, GetOffset get_offset) {
		sort_by_word_len(word_attrib.begin(), word_attrib.end(), get_offset);
	}
	template<class WordAttributs, class GetOffset>
	void sort_by_word_len(const WordAttributs* beg, const WordAttributs* end, GetOffset get_offset) {
		assert(ac->n_words + 1  == size_t(end-beg));
		assert(all_words_size == size_t(get_offset(end[-1])));
		const auto* states = &ac->states[0];
		word_id_t * output = &ac->output[0];
		for (size_t i = 0, n = ac->states.size()-1; i < n; ++i) {
			size_t out0 = states[i+0].output;
			size_t out1 = states[i+1].output;
			assert(out0 <= out1);
			if (out1 - out0 > 1) {
				std::sort(output+out0, output+out1,
				[=](word_id_t x, word_id_t y) {
					size_t xoff0 = get_offset(beg[x+0]);
					size_t xoff1 = get_offset(beg[x+1]);
					size_t yoff0 = get_offset(beg[y+0]);
					size_t yoff1 = get_offset(beg[y+1]);
					assert(xoff0 < xoff1); // assert not be empty
					assert(yoff0 < yoff1); // assert not be empty
					return xoff1 - xoff0 > yoff1 - yoff0; // greater
				});
			}
		}
	}
};

template<class DaTrie>
class DoubleArray_AC_RestoreWord : public Aho_Corasick<DaTrie> {
public:
	using Aho_Corasick<DaTrie>::restore_word;
	void restore_word(size_t stateID, size_t wID, std::string* word)
	const override final {
		assert(this->states.size() > 1);
		assert(this->offsets.size() > 1);
		assert(stateID != initial_state);
		assert(stateID < this->states.size()-1);
		assert(wID < this->offsets.size()-1);
		size_t len = this->wlen(wID);
		size_t child = stateID;
		word->resize(len);
		char* p = &*word->begin() + len;
		auto  lstates = this->states.data();
		for(size_t i = 0; i < len; ++i) {
			assert(child != initial_state);
			size_t parent = lstates[child].parent();
			assert(parent < this->states.size()-1);
			size_t first_child = lstates[parent].child0();
			assert(first_child <= child);
			size_t ch = child - first_child;
			*--p = char(ch); // reverse copy
			child = parent;
		}
	}
	template<class DataIO>
	friend void
	DataIO_loadObject(DataIO& dio, DoubleArray_AC_RestoreWord& x) {
		DataIO_loadObject(dio, static_cast<Aho_Corasick<DaTrie>&>(x));
	}
	template<class DataIO>
	friend void
	DataIO_saveObject(DataIO& dio, const DoubleArray_AC_RestoreWord& x) {
		DataIO_saveObject(dio, static_cast<const Aho_Corasick<DaTrie>&>(x));
	}
};

typedef DoubleArray_AC_RestoreWord<DoubleArrayTrie<AC_State<DA_State8B> > >
		Aho_Corasick_DoubleArray_8B;

typedef Aho_Corasick<AutomataAsBaseDFA<AC_State<State32> > > Aho_Corasick_SmartDFA_AC_State32; // 16 Bytes
typedef Aho_Corasick<AutomataAsBaseDFA<AC_State<State4B> > > Aho_Corasick_SmartDFA_AC_State4B; //  8 Bytes

#if !defined(_MSC_VER)
typedef Aho_Corasick<AutomataAsBaseDFA<AC_State<State5B> > > Aho_Corasick_SmartDFA_AC_State5B; // 12 Bytes
#endif

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

template<class AcTrie>
class FullDFA_BaseOnAC : public BaseDFA {
public:
	typedef typename AcTrie::transition_t transition_t;
	typedef typename AcTrie::state_id_t state_id_t;
	typedef typename AcTrie::state_t    state_t;

	static const state_id_t max_state = AcTrie::max_state;
	static const state_id_t nil_state = AcTrie::nil_state;
	enum { sigma = AcTrie::sigma };

	const AcTrie* m_ac;
	static const size_t m_atom_dfa_num = 0;
	static const size_t m_dfa_cluster_num = 0;

	FullDFA_BaseOnAC() : m_ac(NULL) {
		m_dyn_sigma = 256;
		m_is_dag = false;
	}
	explicit FullDFA_BaseOnAC(const AcTrie* ac) : m_ac(ac) {
		m_dyn_sigma = ac->get_sigma();
		m_is_dag = false;
	}

	size_t total_states() const { return m_ac->total_states() - 1; }
	size_t num_zpath_states() const { return 0; }
	size_t total_transitions() const { return 256 * this->total_states(); }
	size_t mem_size() const override final { return m_ac->mem_size(); }
	size_t v_gnode_states() const override final { return this->total_states(); }

	fstring get_zpath_data(size_t, MatchContext*) const {
		THROW_STD(invalid_argument, "Invalid call");
		return fstring(); // remove compiler warning
	}
	template<class... Args>
	void copy_from(const Args&... args) {
		THROW_STD(invalid_argument, "This is a stub function, should not be called");
	}

	size_t v_num_children(size_t) const override { TERARK_DIE("should not call"); }
	bool v_has_children(size_t s) const override { return m_ac->v_has_children(s); }

	bool has_freelist() const override { return m_ac->has_freelist(); }
	bool is_free(size_t state) const { return m_ac->is_free(state); }
	bool is_pzip(size_t state) const { return false; }
	bool is_term(size_t state) const { return m_ac->full_dfa_is_term(state); }

	size_t state_move(size_t state, auchar_t ch) const {
		return m_ac->full_dfa_state_move(state, ch);
	}
	template<class OP>
	void for_each_move(state_id_t state, OP op) const {
		return m_ac->template full_dfa_for_each_move<OP>(state, op);
	}
	template<class OP>
	void for_each_dest(state_id_t state, OP op) const {
		return m_ac->template full_dfa_for_each_dest<OP>(state, op);
	}
	template<class OP>
	void for_each_dest_rev(state_id_t state, OP op) const {
		return m_ac->template full_dfa_for_each_dest_rev<OP>(state, op);
	}

	friend size_t dfa_matchid_root(const FullDFA_BaseOnAC* dfa, size_t state) {
		return dfa->is_term(state) ? state : dfa->nil_state;
	}
	friend size_t dfa_read_matchid(const FullDFA_BaseOnAC* dfa, size_t state, valvec<int>* vec) {
		return dfa->m_ac->full_dfa_read_matchid(state, vec);
	}

	typedef FullDFA_BaseOnAC MyType;
//#include "ppi/for_each_suffix.hpp"
//#include "ppi/match_key.hpp"
//#include "ppi/match_path.hpp"
//#include "ppi/match_prefix.hpp"
#include "ppi/dfa_const_virtuals.hpp"
#include "ppi/state_move_fast.hpp"
//#include "ppi/for_each_word.hpp"

	void finish_load_mmap(const DFA_MmapHeader*) override {
		THROW_STD(invalid_argument, "Unsupported");
	}
	long prepare_save_mmap(DFA_MmapHeader*, const void**) const override {
		THROW_STD(invalid_argument, "Unsupported");
	}
};

template<class StateID>
class AC_FullDFA : public FullDFA<StateID>, public BaseAC {
	typedef FullDFA<StateID> super;
	using super::TermBit;
	using super::m_index_map;
	using super::states;
	using super::get_map_id;

public:
	typedef StateID state_id_t;

	const BaseAC* get_ac() const override final { return this; }

	~AC_FullDFA() {
		if (this->mmap_base) {
			offsets.risk_release_ownership();
			strpool.risk_release_ownership();
		}
	}

	void risk_swap(AC_FullDFA& y) {
		super::risk_swap(y);
		BaseAC::swap(y);
	}
	void swap(AC_FullDFA& y) {
		assert(typeid(*this) == typeid(y));
		risk_swap(y);
	}

	template<class AC, class SrcID>
	void build_from(const FullDFA_BaseOnAC<AC>& fullac, const SrcID* src_roots, size_t num_roots) {
		super::build_from(fullac, src_roots, num_roots);
		BaseAC::assign(*fullac.m_ac);
	}

	void ac_scan(fstring input, const OnHitWords& on_hit)
	const override final {
		tpl_ac_scan(input, on_hit);
	}
	void ac_scan(fstring input, const OnHitWords& on_hit, const ByteTR& tr)
	const override final {
		tpl_ac_scan(input, on_hit, tr);
	}
	void ac_scan(ByteInputRange& r, const OnHitWords& on_hit)
	const override final {
		scan_stream(&r, &on_hit);
	}
	void ac_scan(ByteInputRange& r, const OnHitWords& on_hit, const ByteTR& tr)
	const override final {
		scan_stream(&r, &on_hit, &tr);
	}

	const word_id_t* words(state_id_t state, size_t* num_words) const {
		assert(state + 2 < states.size());
		assert(states[state] & TermBit);
		size_t map_id = get_map_id(states[state]);
		assert(map_id < m_index_map.size());
		size_t n_uniq = m_index_map[map_id].n_uniq;
		size_t n_match = states[state + 1 + n_uniq];
		*num_words = n_match;
		return &states[states + 2 + n_uniq];
	}

	// res[i].first : KeyWord id
	// res[i].second: end position of str
	// Aho-Corasick don't save strlen(word_id)
	template<class Pos>
	void tpl_ac_scan(fstring str, valvec<std::pair<word_id_t, Pos> >* res) const {
		tpl_ac_scan(str, [=](size_t endpos, const word_id_t* hits, size_t n, size_t) {
						for (size_t i = 0; i < n; ++i) {
							std::pair<word_id_t, Pos> x;
							x.first = hits[i];
							x.second = Pos(endpos);
							res->push_back(x);
						}
					});
	}

	template<class OnHitWord>
	void tpl_ac_scan(fstring str, OnHitWord on_hit_word) const {
		tpl_ac_scan<OnHitWord, IdentityTR>(str, on_hit_word, IdentityTR());
	}

	template<class OnHitWord, class TR>
	void tpl_ac_scan(const fstring str, OnHitWord on_hit_word, TR tr) const {
		assert(this->n_words > 0);
		size_t curr = initial_state;
		auto lstates = states.data();
		for(size_t pos = 0; ; ++pos) {
			size_t map_id = this->get_map_id(lstates[curr]);
			auto&  ent = m_index_map[map_id];
			if (lstates[curr] & TermBit) {
				size_t oBeg = curr + 2 + ent.n_uniq;
				size_t oNum = lstates[oBeg - 1];
				assert(oBeg + oNum <= states.size());
				size_t end_pos_of_hit = pos;
				on_hit_word(end_pos_of_hit, lstates + oBeg, oNum, curr);
			}
			if (pos < size_t(str.n)) {
				byte_t ch = (byte_t)tr((byte_t)str.p[pos]);
				curr = lstates[curr + 1 + ent.idxmap[ch]];
			} else
				break;
		}
	}

	///@param on_hit_word(size_t end_pos_of_hit, const word_id_t* first, size_t num)
	template<class InputStream, class OnHitWord>
	void scan_stream(InputStream* input, OnHitWord* on_hit_word) const {
		scan_stream<InputStream&, OnHitWord&>(*input, *on_hit_word, IdentityTR());
	}

	///@param on_hit_word(size_t end_pos_of_hit, const word_id_t* first, size_t num)
	template<class InputStream, class OnHitWord, class TR>
	void scan_stream(InputStream* input, OnHitWord* on_hit_word, TR* tr) const {
		scan_stream<InputStream&, OnHitWord&, TR&>(*input, *on_hit_word, *tr);
	}

	template<class InputStream, class OnHitWord>
	void scan_stream(InputStream input, OnHitWord on_hit_word) const {
		scan_stream<InputStream, OnHitWord>(input, on_hit_word, IdentityTR());
	}
	template<class InputStream, class OnHitWord, class TR>
	void scan_stream(InputStream input, OnHitWord on_hit_word, TR tr) const {
		assert(this->n_words > 0);
		size_t curr = initial_state;
		auto lstates = states.data();
		for(size_t pos = 0; ; ++pos) {
			size_t map_id = this->get_map_id(lstates[curr]);
			auto&  ent = m_index_map[map_id];
			if (lstates[curr] & TermBit) {
				size_t oBeg = curr + 2 + ent.n_uniq;
				size_t oNum = lstates[oBeg - 1];
				assert(oBeg + oNum <= states.size());
				size_t end_pos_of_hit = pos;
				on_hit_word(end_pos_of_hit, lstates + oBeg, oNum, curr);
			}
			if (input.empty())
				break;
			byte_t c = tr(*input); ++input;
			curr = lstates[curr + 1 + ent.idxmap[c]];
		}
	}

	void finish_load_mmap(const DFA_MmapHeader* base) override {
		super::finish_load_mmap(base);
		assert(base->num_blocks >= 2 + base->ac_word_ext);
		assert(base->ac_word_ext <= 2);
		BaseAC::ac_str_finish_load_mmap(base);
	}

	long prepare_save_mmap(DFA_MmapHeader* base, const void** dataPtrs)
	const override final {
		super::prepare_save_mmap(base, dataPtrs);
		BaseAC::ac_str_prepare_save_mmap(base, dataPtrs);
		return 0;
	}

	template<class DataIO>
	friend void DataIO_loadObject(DataIO&, AC_FullDFA&) {
		THROW_STD(invalid_argument, "Unsupported");
	}
	template<class DataIO>
	friend void DataIO_saveObject(DataIO&, const AC_FullDFA&) {
		THROW_STD(invalid_argument, "Unsupported");
	}
};

} // namespace terark

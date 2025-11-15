#pragma once

#include <assert.h>
#include <stdio.h>
#include <terark/fstring.hpp>
#include <terark/valvec.hpp>
#include <terark/util/function.hpp>

namespace terark {

class BaseDFA;

// life time should be longer than MultiRegexSubmatch/MultiRegexFullMatch
// can be used by multiple MultiRegexSubmatch/MultiRegexFullMatch objects
struct TERARK_DLL_EXPORT MultiRegexMatchOptions {
	std::string dfaFilePath;
	std::string regexMetaFilePath;
	size_t maxBitmapSize;
	bool enableDynamicDFA;

	~MultiRegexMatchOptions();
	MultiRegexMatchOptions();
	MultiRegexMatchOptions(fstring dfaFilePath);

	void load_dfa();
	void load_dfa(fstring dfaFilePath);
	void load_dfa_user_mem(fstring user_mem);
	BaseDFA* get_dfa() const { return m_dfa; }
private:
	bool m_owns_dfa = true;
	BaseDFA* m_dfa;
private: // disable copy
	MultiRegexMatchOptions(const MultiRegexMatchOptions&) = delete;
	MultiRegexMatchOptions& operator=(const MultiRegexMatchOptions&) = delete;
};

class TERARK_DLL_EXPORT MultiRegexSubmatch {
protected:
	typedef function<byte_t(byte_t)> ByteTR;
	const BaseDFA* dfa;
	valvec<int > m_fullmatch_regex;
	valvec<int > cap_pos_data;
	valvec<int*> cap_pos_ptr;
	class MultiRegexFullMatch* m_first_pass;

	void fix_utf8_boundary(fstring text);

	MultiRegexSubmatch();
	MultiRegexSubmatch(const MultiRegexSubmatch& y);
public:
	static  MultiRegexSubmatch* create(const MultiRegexMatchOptions&);
	virtual MultiRegexSubmatch* clone() const = 0;

	virtual ~MultiRegexSubmatch();

	virtual void warm_up();

	virtual size_t match(fstring text) = 0;
	virtual size_t match(fstring text, const ByteTR& tr) = 0;
	virtual size_t match(fstring text, const byte_t* tr) = 0;

	size_t match_utf8(fstring text);
	size_t match_utf8(fstring text, const ByteTR& tr);
	size_t match_utf8(fstring text, const byte_t* tr);

	///@{ only for internal use
	void push_regex_info(int n_submatch);
	void complete_regex_info();
	void reset();
	void reset(int regex_id);
	///@}

	int num_submatch(int regex_id) const {
		assert(regex_id >= 0);
		assert(regex_id < (int)cap_pos_data.size());
		int* p0 = cap_pos_ptr[regex_id+0];
		int* p1 = cap_pos_ptr[regex_id+1];
		int  ns = p1 - p0;
		assert(ns % 2 == 0);
		return ns / 2;
	}

	bool is_full_match(int regex_id) const {
		assert(regex_id >= 0);
		assert(regex_id < (int)cap_pos_data.size());
		return -1 != cap_pos_ptr[regex_id][1];
	}

	std::pair<int,int>
	get_match_range(int regex_id, int sub_id) const;

	// easy to get a submatch
	fstring sub(const char* base, int regex_id, int sub_id) const {
		return (*this)(base, regex_id, sub_id);
	}
	fstring operator()(const char* base, int regex_id, int sub_id) const;

	const valvec<int>& fullmatch_regex() const {
		return m_fullmatch_regex;
	}

	size_t max_partial_match_len() const;
};

/// base class valvec<int> is used for saving matched regex id(s)
class TERARK_DLL_EXPORT MultiRegexFullMatch {
public:
#pragma pack(push,1)
	struct PosLen {
		int pos;
		int len;
		int endpos() const { return pos + len; }
		PosLen(int pos1, int len1)
			: pos(pos1), len(len1) {}
	};
	struct LenRegexID {
		int len;
		int regex_id;
		LenRegexID() = default;
		LenRegexID(int len1, int regex_id1) : len(len1), regex_id(regex_id1) {}
		friend bool operator==(const LenRegexID& x, const LenRegexID& y)
		  { return x.len == y.len && x.regex_id == y.regex_id; }
		friend bool operator!=(const LenRegexID& x, const LenRegexID& y)
		  { return x.len != y.len || x.regex_id != y.regex_id; }
		struct Collector {
			valvec<LenRegexID>* vec;
			int len;
			void push_back(int regex_id) { vec->push_back({len, regex_id}); }
			void append(const int* src, size_t num) {
				LenRegexID* dst = vec->grow(num);
				for (size_t i = 0; i < num; i++) dst[i] = {len, src[i]};
			}
			size_t size() const { return vec->size(); } // for debug
			const LenRegexID& operator[](size_t i) const { return (*vec)[i]; }
		};
	};
	struct PosLenRegexID {
		int pos;
		int len;
		int regex_id;
		PosLenRegexID(int pos1, int len1, int regex_id1)
			: pos(pos1), len(len1), regex_id(regex_id1) {}
	};
#pragma pack(pop)
protected:
	typedef function<byte_t(byte_t)> ByteTR;
    valvec<int>  m_regex_idvec;
	valvec<PosLenRegexID> m_all_match;
	valvec<LenRegexID>    m_cur_match; // at pos 0
	const BaseDFA*  m_dfa;
	class DenseDFA_DynDFA_256* m_dyn;
	const MultiRegexMatchOptions* m_options;
	size_t m_max_partial_match_len;
	MultiRegexFullMatch();
	MultiRegexFullMatch(const MultiRegexFullMatch&);
public:
    const int* begin() const { return m_regex_idvec.begin(); }
    const int* end() const { return m_regex_idvec.end(); }
    size_t size() const { return m_regex_idvec.size(); }
    bool  empty() const { return m_regex_idvec.empty(); }
    int regex_id(size_t idx) const {
        assert(idx < m_regex_idvec.size());
        return m_regex_idvec[idx];
    }
    /// only for internal use
    valvec<int>& mutable_regex_idvec() { return m_regex_idvec; }

	virtual ~MultiRegexFullMatch();
	virtual	MultiRegexFullMatch* clone() const = 0;
	static  MultiRegexFullMatch* create(const MultiRegexMatchOptions&);

	class DenseDFA_DynDFA_256* get_dyn_dfa() const { return m_dyn; }
	void warm_up();
	void set_maxmem(size_t maxmem);
	size_t get_maxmem() const;

    ///@{ longest match from start, return value is matched len
	virtual size_t match(fstring text) = 0;
	virtual size_t match(fstring text, const ByteTR& tr) = 0;
	virtual size_t match(fstring text, const byte_t* tr) = 0;
    ///@}

    ///@{ shortest match from start, return value is matched len
	virtual size_t shortest_match(fstring text) = 0;
	virtual size_t shortest_match(fstring text, const ByteTR& tr) = 0;
	virtual size_t shortest_match(fstring text, const byte_t* tr) = 0;
    ///@}

    ///@{ match from start, get all id of all matched regex-es which
    ///   may be ended at any position
    ///@returns number of matched regex, same as this->size()
	virtual size_t match_all(fstring text) = 0;
	virtual size_t match_all(fstring text, const ByteTR& tr) = 0;
	virtual size_t match_all(fstring text, const byte_t* tr) = 0;
    ///@}

    ///@{ match from start, get all id of all matched regex-es which
    ///   may be ended at any position
    ///@returns number of matched regex, same as this->size()
	virtual size_t match_all_len(fstring text) = 0;
	virtual size_t match_all_len(fstring text, const ByteTR& tr) = 0;
	virtual size_t match_all_len(fstring text, const byte_t* tr) = 0;
    ///@}

	virtual PosLen utf8_find_first(fstring text);
	virtual PosLen utf8_find_first(fstring text, const ByteTR& tr);
	virtual PosLen utf8_find_first(fstring text, const byte_t* tr);

	virtual PosLen shortest_utf8_find_first(fstring text);
	virtual PosLen shortest_utf8_find_first(fstring text, const ByteTR& tr);
	virtual PosLen shortest_utf8_find_first(fstring text, const byte_t* tr);

	virtual PosLen byte_find_first(fstring text);
	virtual PosLen byte_find_first(fstring text, const ByteTR& tr);
	virtual PosLen byte_find_first(fstring text, const byte_t* tr);

	virtual PosLen shortest_byte_find_first(fstring text);
	virtual PosLen shortest_byte_find_first(fstring text, const ByteTR& tr);
	virtual PosLen shortest_byte_find_first(fstring text, const byte_t* tr);

	virtual size_t utf8_find_all(fstring text);
	virtual size_t utf8_find_all(fstring text, const ByteTR& tr);
	virtual size_t utf8_find_all(fstring text, const byte_t* tr);

	virtual size_t shortest_utf8_find_all(fstring text);
	virtual size_t shortest_utf8_find_all(fstring text, const ByteTR& tr);
	virtual size_t shortest_utf8_find_all(fstring text, const byte_t* tr);

	virtual size_t byte_find_all(fstring text);
	virtual size_t byte_find_all(fstring text, const ByteTR& tr);
	virtual size_t byte_find_all(fstring text, const byte_t* tr);

	virtual size_t shortest_byte_find_all(fstring text);
	virtual size_t shortest_byte_find_all(fstring text, const ByteTR& tr);
	virtual size_t shortest_byte_find_all(fstring text, const byte_t* tr);

    virtual size_t utf8_find_all_len(fstring text);
	virtual size_t utf8_find_all_len(fstring text, const ByteTR& tr);
	virtual size_t utf8_find_all_len(fstring text, const byte_t* tr);
	virtual size_t byte_find_all_len(fstring text);
	virtual size_t byte_find_all_len(fstring text, const ByteTR& tr);
	virtual size_t byte_find_all_len(fstring text, const byte_t* tr);

	virtual bool has_hit(int regex_id) const;

	virtual void clear_match_result();

	valvec<PosLenRegexID>& mutable_all_match() { return m_all_match; }
	const valvec<PosLenRegexID>& all_match() const { return m_all_match; }
	const PosLenRegexID& all_match(size_t i) const { return m_all_match[i]; }
	size_t all_match_size() const { return m_all_match.size(); }
	const valvec<LenRegexID>& cur_match() const { return m_cur_match; }
	size_t max_partial_match_len() const { return m_max_partial_match_len; }
	const BaseDFA* get_dfa() const { return m_dfa; }
};

inline
size_t MultiRegexSubmatch::max_partial_match_len() const {
	assert(m_first_pass);
   	return m_first_pass->max_partial_match_len();
}

} // namespace terark

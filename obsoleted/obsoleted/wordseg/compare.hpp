/* vim: set tabstop=4 : */
#ifndef __terark_wordseg_compare_hpp__
#define __terark_wordseg_compare_hpp__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#include <assert.h>
#include <wctype.h>
#include <string.h>
#include <string>
#include <algorithm>
#include <iostream>
#include <vector>

#include "../const_string.hpp"
#include <terark/util/compare.hpp>
#include "gb2312_ctype.hpp"

namespace terark { namespace wordseg {

#ifdef _MSC_VER
//#  pragma warning(disable: 4996)
#  define wcsncasecmp _wcsnicmp
#  define strncasecmp _strnicmp
#endif

SAME_NAME_MEMBER_COMPARATOR_EX(CompareFirstBegin, const wchar_t*, const wchar_t*, .first.begin(), std::less<const wchar_t*>)
SAME_NAME_MEMBER_COMPARATOR_EX(CompareBegin, const wchar_t*, const wchar_t*, .begin(), std::less<const wchar_t*>)

TERARK_DLL_EXPORT std::wstring ss_cvt(const_string x);
TERARK_DLL_EXPORT std::string ss_cvt(const_wstring x);

TERARK_DLL_EXPORT int strncasecoll(const char* x, const char* y, size_t n1, size_t n2);
TERARK_DLL_EXPORT int strncoll(const char* x, const char* y, size_t n1, size_t n2);

class CompareWordsW
{
protected:
	typedef int (*fstrcomp_t)(const wchar_t*, const wchar_t*, size_t);
	fstrcomp_t fstrcomp;

public:
	CompareWordsW(fstrcomp_t fstrcomp = wcsncasecmp)
		: fstrcomp(fstrcomp)
	{
	}

	template<class T1, class T2>
	bool operator()(const T1& x, const T2& y) const
	{
		return compare(x, y) < 0;
	}
	template<class T1, class T2>
	int compare(const T1& x, const T2& y) const
	{
		using namespace std;
		size_t nx = x.size(), ny = y.size();
		int ret = fstrcomp(x.data(), y.data(), min(nx, ny));
		return 0 == ret ? nx - ny : ret;
	}
};

class CompareWordsA
{
protected:
	typedef int (*fstrcomp_t)(const char*, const char*, size_t, size_t);
	fstrcomp_t fstrcomp;

public:
	CompareWordsA(fstrcomp_t fstrcomp = strncasecoll)
		: fstrcomp(fstrcomp)
	{
	}

	template<class T1, class T2>
	bool operator()(const T1& x, const T2& y) const
	{
		return compare(x, y) < 0;
	}
	template<class T1, class T2>
	int compare(const T1& x, const T2& y) const
	{
		using namespace std;
		size_t nx = x.size(), ny = y.size();
		int ret = fstrcomp(x.data(), y.data(), nx, ny);
		return 0 == ret ? nx - ny : ret;
	}
};


class CompareWordsBackward
{
public:
	template<class T1, class T2>
	bool operator()(const T1& x, const T2& y) const
	{
		return compare(x, y) < 0;
	}
	template<class Iter1, class Iter2>
	int compare(Iter1 x_end, Iter2 y_end, int xsize, int ysize) const
	{
		using namespace std;
		int n = min(xsize, ysize);
		while (n)
		{
			--x_end; --y_end; --n;
			const wchar_t cx = towlower(*x_end);
			const wchar_t cy = towlower(*y_end);
			if (cx != cy)
				return cx - cy;
		}
		return xsize - ysize;
	}
	template<class T1, class T2>
	int compare(const T1& x, const T2& y) const
	{
		return compare(x.end(), y.end(), x.size(), y.size());
	}
};

class GetCharAtPos
{
public:
	template<class CharT>
	CharT operator()(const CharT* beg, int pos) const { return beg[pos]; }
};

class GetCharFromBegin : public GetCharAtPos
{
public:
	template<class CharT>
	CharT operator()(const basic_const_string<CharT>& word, int pos) const
	{
		return word[pos];
	}
	using GetCharAtPos::operator();
};
class GetCharFromEnd : public GetCharAtPos
{
public:
	template<class CharT>
	CharT operator()(const basic_const_string<CharT>& word, int pos) const
	{
		assert(-pos <= int(word.size()));
		return word.end()[pos];
	}
	using GetCharAtPos::operator();
};
class GetCharFromEnd_ForSort
{
public:
	template<class CharT>
	CharT operator()(const basic_const_string<CharT>& word, int pos) const
	{
		assert(pos < int(word.size()));
		return word.end()[-pos-1];
	}
};

class char_tolower
{
public:
	wchar_t operator()(wchar_t wch) const {	return towlower(wch); }
//	wint_t  operator()(wint_t  wch) const {	return towlower(wch); }
	char operator()(char ch) const { return tolower(ch); }
};

struct CharLess
{
	template<class CharT> bool operator()(CharT x, CharT y) const { return x < y; }
	template<class CharT> int compare(CharT x, CharT y) const { return x - y; }
};

struct CharGreater
{
	template<class CharT> bool operator()(CharT x, CharT y) const { return y < x; }
	template<class CharT> int compare(CharT x, CharT y) const { return y - x; }
};

//! CompareChar only can be CharLess or CharGreater
//!
//! do not change operator() and compare(CharT x, CharT y) inherited from CompareCharCode
//! just add bi_comp and compare(string x, string y)
//!
template< class CharT
		, class GetCharElem = GetCharFromBegin
		, class GetCharCode = XIdentity
		, class CompareCharCode = CharLess
		>
class StrCharPos_Compare : public CompareCharCode
{
protected:
	GetCharElem m_gch;
	GetCharCode m_code;

public:
	typedef CharT char_t;
	typedef basic_const_string<CharT> const_str_t;
	typedef std::basic_string<CharT> str_t;

	explicit StrCharPos_Compare(const GetCharElem& gch = GetCharElem()
							  , const GetCharCode& code = GetCharCode()
							   )
	: m_gch(gch), m_code(code) {}

	bool chEqual(CharT x, CharT y) const
	{
		return m_code(x) == m_code(y);
	}

	CharT code(CharT x) const
	{
		return m_code(x);
	}
	CharT code(const_str_t x, int pos) const
	{
		return m_code(m_gch(x, pos));
	}
	CharT code(str_t x, int pos) const
	{
		return m_code(m_gch(x, pos));
	}

	template<class T1, class T2>
	int prefix(const T1& x, const T2& y, int pos, int bound) const
	{
		int i = pos;
		while (i < bound && chEqual(x[i], y[i]))
		{
			++i;
		}
		return i;
	}

	int suffix(const CharT* x, const CharT* y, int pos, int bound) const
	{
		assert(bound < 0);
		int i = pos;
		while (i >= bound && chEqual(x[i], y[i]))
		{
			--i;
		}
		return i;
	}

	bool bi_comp(const const_str_t& x, const const_str_t& y, int pos) const
	{
		assert(pos < int(x.size()));
		assert(pos < int(y.size()));
		return CompareCharCode::operator()(code(x,pos), code(y, pos));
	}
	int compare(const const_str_t& x, const const_str_t& y, int pos) const
	{
		assert(pos < int(x.size()));
		assert(pos < int(y.size()));
		return CompareCharCode::compare(code(x,pos), code(y, pos));
	}

	bool bi_comp(const str_t& x, const str_t& y, int pos) const
	{
		assert(pos < int(x.size()));
		assert(pos < int(y.size()));
		return CompareCharCode::operator()(code(x,pos), code(y, pos));
	}
	int compare(const str_t& x, const str_t& y, int pos) const
	{
		assert(pos < int(x.size()));
		assert(pos < int(y.size()));
		return CompareCharCode::compare(code(x,pos), code(y, pos));
	}

	bool bi_comp(const CharT* x, const CharT* y, int pos) const
	{
		return CompareCharCode::operator()(code(x,pos), code(y, pos));
	}
	int compare(const CharT* x, const CharT* y, int pos) const
	{
		return CompareCharCode::compare(code(x,pos), code(y, pos));
	}

	bool bi_comp(const CharT x, const CharT y) const
	{
		return CompareCharCode::operator()(m_code(x), m_code(y));
	}

//	using CompareCharCode::operator()
// 	int compare(const CharT x, const CharT y) const
// 	{
// 		return CompareCharCode::compare(m_code(x), m_code(y));
// 	}
	using CompareCharCode::compare;
};

/*
template< class CharT
		, class GetCharElem = GetCharFromBegin
		, class GetCharCode = XIdentity
		, class GetStringEl = XIdentity
		, class CompareCharCode = CharLess
		>
class StrCharPos_GenStrCmp
{
	typedef StrCharPos_Compare<CharT, GetCharElem, GetCharCode, CompareCharCode> comp_t;
	int         m_beg_pos;
	int         m_end_pos;
	comp_t      m_comp;
	GetStringEl m_str;

public:
	typedef CharT char_t;
	typedef basic_const_string<CharT> const_str_t;
	typedef std::basic_string<CharT> str_t;

	explicit StrCharPos_GenStrCmp(int beg_pos, int end_pos
		, const GetCharElem& gch  = GetCharElem()
		, const GetCharCode& code = GetCharCode()
		, const GetStringEl& gstr = GetStringEl()
		)
	: m_beg_pos(beg_pos), m_end_pos(end_pos), m_comp(gch, code), m_str(gstr) {}

	explicit StrCharPos_GenStrCmp(int beg_pos, int end_pos
		, const comp_t& comp
		, const GetStringEl& gstr
		)
	: m_beg_pos(beg_pos), m_end_pos(end_pos), m_comp(comp), m_str(gstr) {}

	template<class S1, class S2>
	int comp_imp(const S1& x, const S2& y) const
	{
		using namespace std;
		int len = min(x.size(), y.size());
		len = min(len, m_end_pos);
		for (int i = m_beg_pos; i < len; ++i)
		{
			CharT cx = m_comp.code(x, i);
			CharT cy = m_comp.code(y, i);
			if (cx != cy)
				return m_comp.compare(cx, cy);
		}
	//	严格的比较结果：
	//	if (m_end_pos == len)
	//		return x.size() - y.size();
	//	else
	//		return 0;

	//	使用简化的比较对结果没有多大影响
		return x.size() - y.size();
	}
	int compare(const const_str_t& x, const const_str_t& y) const { return comp_imp(x,y); }
	int compare(const       str_t& x, const       str_t& y) const { return comp_imp(x,y); }
	int compare(const const_str_t& x, const       str_t& y) const { return comp_imp(x,y); }
	int compare(const       str_t& x, const const_str_t& y) const { return comp_imp(x,y); }

	template<class TX, class TY>
	int compare(const TX& x, const TY& y) const { return compare(m_str(x), m_str(y)); }

	template<class TX, class TY>
	bool operator()(const TX& x, const TY& y) const { return compare(m_str(x), m_str(y)) < 0; }
};
*/

template<class StrCharPos_CompareT>
class StrCharPos_Compare_Adapter
{
	const StrCharPos_CompareT& m_imp;
	int m_pos;

public:
	typedef typename StrCharPos_CompareT::char_t char_t;

	StrCharPos_Compare_Adapter(const StrCharPos_CompareT& imp, int pos)
		: m_imp(imp), m_pos(pos) {}

	template<class Any>
	bool operator()(const Any& x, const Any& y) const
	{
		return m_imp.bi_comp(x, y, m_pos);
	}
	template<class Any>
	bool operator()(const Any& x, const char_t y) const
	{
		return m_imp.bi_comp(x, y, m_pos);
	}
	template<class Any>
	bool operator()(const char_t x, const Any& y) const
	{
		return m_imp.bi_comp(x, y, m_pos);
	}
};

template<class StrCharPos_CompareT>
class StrCharPos_Compare_Adapter_Checked
{
	const StrCharPos_CompareT& m_imp;
	int m_pos;

public:
	typedef typename StrCharPos_CompareT::char_t char_t;

	StrCharPos_Compare_Adapter_Checked(const StrCharPos_CompareT& imp, int pos)
		: m_imp(imp), m_pos(pos) {}

	template<class Any>
	bool operator()(const Any& x, const Any& y) const
	{
		return m_imp.checked_bi_comp(x, y, m_pos);
	}
	template<class Any>
	bool operator()(const Any& x, const char_t y) const
	{
		return m_imp.checked_bi_comp(x, y, m_pos);
	}
	template<class Any>
	bool operator()(const char_t x, const Any& y) const
	{
		return m_imp.checked_bi_comp(x, y, m_pos);
	}
};

} }

#endif // __terark_wordseg_compare_hpp__

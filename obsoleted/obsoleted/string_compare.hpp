/* vim: set tabstop=4 : */
#ifndef __string_compare_hpp__
#define __string_compare_hpp__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#include <ctype.h>
#include <string.h>
#include <string>
#include <functional>
#include <algorithm>

#include "compare.hpp"

// should be the last #include
#include <boost/type_traits/detail/bool_trait_def.hpp>

namespace terark {

/*
template<class CharT>
inline CharT my_tolower(CharT c)
{
	if ('A' <= c&&c <= 'Z')
		return c + 0x20;
	else
		return c;
}
*/

inline char my_tolower(char c)
{
	return tolower((unsigned char)c);
}

template<class CharT>
inline CharT my_tolower(CharT c)
{
	return (CharT)tolower(c);
}

struct StringCompareChar
{
	bool less(char left, char right) const
	{
		return (unsigned char)left < (unsigned char)right;
	//	return left < right;
	}
	bool less(wchar_t left, wchar_t right) const { return left < right; }

	template<class CharT>
	bool equal(CharT left, CharT right) const { return left == right; }

	// 算术运算时会自动扩展，所以 unsigned char 相减时不会溢出
	template<class CharT>
	int compare(CharT left, CharT right) const { return left - right; }
};

struct StringCompareCharNoCase
{
	bool less(char left, char right) const
	{
		return my_tolower((unsigned char)left) < my_tolower((unsigned char)right);
	//	return my_tolower(left) < my_tolower(right);
	}
	bool less(wchar_t left, wchar_t right) const { return my_tolower(left) < my_tolower(right); }

	template<class CharT>
	bool equal(CharT left, CharT right) const { return my_tolower(left) == my_tolower(right); }

	// 算术运算时会自动扩展，所以 unsigned char 相减时不会溢出
	template<class CharT>
	int compare(CharT left, CharT right) const
	{
		return my_tolower(left) - my_tolower(right);
	}
};

/**
 @brief 比较 String 中的一部分，实现类，仅供内部使用

 @param FinalCompare 从该类继承的派生类，FinalCompare 至少需要实现以下成员函数：
	int getStartPos() const;
	int getLength() const;
 */
template<class FinalCompare>
class StringCompareRangeImpl
{
	FinalCompare& self() { return static_cast<FinalCompare&>(*this); }
	const FinalCompare& self() const { return static_cast<const FinalCompare&>(*this); }

	template<class CharT>
	const CharT* getText(const CharT* x) const
	{
		return x + self().getStartPos();
	}

public:
	int compare(const char* left, const char* right) const
	{
		int len = self().getLength();
		left  = getText(left);
		right = getText(right);
		while (len && *left == *right)
			len--, left++, right++;
		return 0 == len ? 0 : (unsigned char)*left - (unsigned char)*right;
	//	return strncmp(getText(left), getText(right), self().getLength());
	}
	int compare(const wchar_t* left, const wchar_t* right) const
	{
		return wcsncmp(getText(left), getText(right), self().getLength());
	}
	template<class CharT, class Traits, class Alloc>
	int compare(const std::basic_string<CharT, Traits, Alloc>& left,
				 const std::basic_string<CharT, Traits, Alloc>& right) const
	{
		return left.compare(self().getStartPos(), self().getLength(),
					 right, self().getStartPos(), self().getLength());
	}
	template<class CharT, class Traits, class Alloc>
	int compare(const std::basic_string<CharT, Traits, Alloc>& left, const CharT* right) const
	{
		return left.compare(self().getStartPos(), self().getLength(),
								  getText(right), self().getLength());
	}
	template<class CharT, class Traits, class Alloc>
	int compare(const CharT* left, const std::basic_string<CharT, Traits, Alloc>& right) const
	{
		return - right.compare(self().getStartPos(), self().getLength(),
									  getText(left), self().getLength());
	}

	template<class StringT1, class StringT2>
	bool operator()(const StringT1& left, const StringT2& right) const
	{
		return compare(left, right) < 0;
	}
};

/**
 @brief 比较字符串的一个范围内的子串：[startPos, length)

 当使用前缀查出 ZStringTable 中一个范围[lower, upper)时，再使用该比较器在该范围内查找更小的范围
 每个字符串比较减少了 startPos 个字符的比较，因为已经知道 [0, startPos) 的字符都是相同的。
 */
class StringCompareRange : public StringCompareRangeImpl<StringCompareRange>
{
public:
	int m_startPos;
	int m_length;

	StringCompareRange(int startPos = 0, int length = 65535)
		: m_startPos(startPos), m_length(length) {}

private:
	friend class StringCompareRangeImpl<StringCompareRange>;
	int getStartPos() const { return m_startPos; }
	int getLength() const { return m_length; }
};
BOOST_TT_AUX_BOOL_TRAIT_CV_SPEC1(HasTriCompare, StringCompareRange, true)

/**
 @brief 只能用在二分查找时，并且只能用一次

 当字符串的公共前缀比较长时，用这类做比较器进行查找最快
 */
template<class CharCompare>
class StringFindingCompare_Aux : private CharCompare
{
	mutable int m_pos0, m_pos1;

public:
	explicit StringFindingCompare_Aux(int startPos = 0)
	{
		m_pos0 = startPos;
		m_pos1 = startPos;
	}
	int pos() const
	{
		using namespace std;
		return min(m_pos0, m_pos1);
	}
	template<class CharT>
	int compare(const CharT* left, const CharT* right) const
	{
		using namespace std;

		int pos = min(m_pos0, m_pos1);

		while (left[pos] && CharCompare::equal(left[pos], right[pos]))
			++pos;
		if (m_pos0 < m_pos1)
			m_pos0 = pos;
		else
			m_pos1 = pos;

		return CharCompare::compare(left[pos], right[pos]);
	}
	template<class CharT, class Traits, class Alloc>
	int compare(const std::basic_string<CharT, Traits, Alloc>& left,
				const std::basic_string<CharT, Traits, Alloc>& right) const
	{
		using namespace std;

		int pos  = min(m_pos0, m_pos1);
		int size = min(left.size(), right.size());
		while (pos < size && CharCompare::equal(left[pos], right[pos]))
			++pos;
		if (m_pos0 < m_pos1)
			m_pos0 = pos;
		else
			m_pos1 = pos;

		if (pos == size) return 0; // equal

		return CharCompare::compare(left[pos], right[pos]);
	}

	template<class StringT1, class StringT2>
	bool operator()(const StringT1& left, const StringT2& right) const
	{
		return compare(left, right) < 0;
	}
};

/**
 @brief 只能用在二分查找时，并且只能用一次

 当字符串的公共前缀比较长时，用这类做比较器进行查找最快
 */
template<class CharCompare>
class StringFindingComparePrefix_Aux : private CharCompare
{
	mutable int m_pos0, m_pos1;

public:
	int m_nPrefix;

	explicit StringFindingComparePrefix_Aux(int nPrefix, int startPos = 0)
	{
		m_nPrefix = nPrefix;
		m_pos0 = startPos;
		m_pos1 = startPos;
	}
	int pos() const
	{
		using namespace std;
		return min(m_pos0, m_pos1);
	}

	template<class CharT>
	int compare(const CharT* left, const CharT* right) const
	{
		using namespace std;

		int pos = min(m_pos0, m_pos1);

		while (pos < m_nPrefix && left[pos] && CharCompare::equal(left[pos], right[pos]))
			++pos;
		if (m_pos0 < m_pos1)
			m_pos0 = pos;
		else
			m_pos1 = pos;

		if (pos == m_nPrefix) return 0; // equal

		return CharCompare::compare(left[pos], right[pos]);
	}

	template<class CharT, class Traits, class Alloc>
	int compare(const std::basic_string<CharT, Traits, Alloc>& left,
				const std::basic_string<CharT, Traits, Alloc>& right) const
	{
		using namespace std;

		int pos  = min(m_pos0, m_pos1);
		int size = min(left.size(), right.size());
		if (size > m_nPrefix) size = m_nPrefix;

		while (pos < size && CharCompare::equal(left[pos], right[pos]))
			++pos;
		if (m_pos0 < m_pos1)
			m_pos0 = pos;
		else
			m_pos1 = pos;

		if (pos == size) return 0; // equal

		return CharCompare::compare(left[pos], right[pos]);
	}
	template<class CharT, class Traits, class Alloc>
	int compare(const std::basic_string<CharT, Traits, Alloc>& left, const CharT* right) const
	{
		return compare(left.c_str(), right);
	}
	template<class CharT, class Traits, class Alloc>
	int compare(const CharT* left, const std::basic_string<CharT, Traits, Alloc>& right) const
	{
		return compare(left.c_str(), right);
	}

	template<class StringT1, class StringT2>
	bool operator()(const StringT1& left, const StringT2& right) const
	{
		return compare(left, right) < 0;
	}
};

typedef StringFindingCompare_Aux<StringCompareChar>		  StringFindingCompare;
typedef StringFindingCompare_Aux<StringCompareCharNoCase> StringFindingCompareNoCase;

typedef StringFindingComparePrefix_Aux<StringCompareChar>		StringFindingComparePrefix;
typedef StringFindingComparePrefix_Aux<StringCompareCharNoCase> StringFindingComparePrefixNoCase;

BOOST_TT_AUX_BOOL_TRAIT_CV_SPEC1(HasTriCompare, StringFindingCompare,			  true)
BOOST_TT_AUX_BOOL_TRAIT_CV_SPEC1(HasTriCompare, StringFindingCompareNoCase,		  true)
BOOST_TT_AUX_BOOL_TRAIT_CV_SPEC1(HasTriCompare, StringFindingComparePrefix,		  true)
BOOST_TT_AUX_BOOL_TRAIT_CV_SPEC1(HasTriCompare, StringFindingComparePrefixNoCase, true)

/**
 @brief 前缀比较器

 实际上是 startPos 为 0 的 range 比较器，但是经过优化，效率更高，
 更因为 StringComparePrefix 的使用频率更高
 */
class StringComparePrefix : public StringCompareRangeImpl<StringComparePrefix>
{
public:
	int m_nPrefix;

	explicit StringComparePrefix(int nPrefix = 1) : m_nPrefix(nPrefix) {}

	void setPrefix(int nPrefix) { m_nPrefix = nPrefix; }
	int  getPrefix() const { return m_nPrefix; }

	template<class CharT, class Traits, class Alloc>
	static size_t common_prefix_length(const std::basic_string<CharT, Traits, Alloc>& s1,
								const std::basic_string<CharT, Traits, Alloc>& s2)
	{
		using namespace std;
		size_t len = 0;
		size_t size = min(s1.size(), s2.size());
		for (; s1[len] == s2[len] && len < size; ++len)
		{
			; // do nothing
		}
		return len;
	}

private:
	friend class StringCompareRangeImpl<StringComparePrefix>;
	int getStartPos() const { return 0; }
	int getLength() const { return m_nPrefix; }
};
BOOST_TT_AUX_BOOL_TRAIT_CV_SPEC1(HasTriCompare, StringComparePrefix, true)

class StringCompareSZ
{
public:
	typedef StringComparePrefix prefix_compare;
	typedef StringCompareRange   range_compare;

	int compare(const char* left, const char* right) const
	{
#ifdef _MSC_VER
//		return _mbscmp(left, right);
#else
#endif
		while (*left == *right)
			++left, ++right;
		return (unsigned char)(*left) - (unsigned char)(*right);
	}
	int compare(const wchar_t* left, const wchar_t* right) const
	{
		return wcscmp(left, right);
	}
	template<class CharT, class Traits, class Alloc>
	int compare(const std::basic_string<CharT, Traits, Alloc>& left,
				const std::basic_string<CharT, Traits, Alloc>& right) const
	{
		return left.compare(right);
	}
	template<class CharT, class Traits, class Alloc>
	int compare(const std::basic_string<CharT, Traits, Alloc>& left, const CharT* right) const
	{
		return left.compare(right);
	}
	template<class CharT, class Traits, class Alloc>
	int compare(const CharT* left, const std::basic_string<CharT, Traits, Alloc>& right) const
	{
		return - right.compare(left);
	}

	template<class StringT1, class StringT2>
	bool operator()(const StringT1& left, const StringT2& right) const
	{
		return compare(left, right) < 0;
	}
};
BOOST_TT_AUX_BOOL_TRAIT_CV_SPEC1(HasTriCompare, StringCompareSZ, true)

/**
 @brief 比较 String 中的一部分，实现类，仅供内部使用

 @param FinalCompare 从该类继承的派生类，FinalCompare 至少需要实现以下成员函数：
	int getStartPos() const;
	int getLength() const;
 */
template<class FinalCompare>
class StringCompareRangeNoCaseImpl
{
	FinalCompare& self() { return static_cast<FinalCompare&>(*this); }
	const FinalCompare& self() const { return static_cast<const FinalCompare&>(*this); }

	template<class CharT>
	const CharT* getText(const CharT* x) const
	{
		return x + self().getStartPos();
	}

public:
	int compare(const char* left, const char* right) const
	{
		const unsigned char *x = (const unsigned char*)getText(left);
		const unsigned char *y = (const unsigned char*)getText(right);
		int count = self().getLength();
		while (count && *x && *y)
		{
		//	if (*x & 0x80 || *y & 0x80)
			if (*x & 0x80)
			{
				if (*x != *y) return *x - *y;
				x++, y++; count--;

				if (*x != *y) return *x - *y;
			}
			else if (my_tolower(*x) != my_tolower(*y))
			{
				return my_tolower(*x) - my_tolower(*y);
			}
			x++, y++; count--;
		}
		return 0 == count ? 0 : my_tolower(*x) - my_tolower(*y);
	}
	int compare(const wchar_t* left, const wchar_t* right) const
	{
		int count = self().getLength();
		const wchar_t *x = getText(left);
		const wchar_t *y = getText(right);
		for (; count && *x && *y; ++x, ++y, --count)
		{
			if (my_tolower(*x) != my_tolower(*y))
				return my_tolower(*x) - my_tolower(*y);
		}
		return 0 == count ? 0 : my_tolower(*x) - my_tolower(*y);
	}

	template<class CharT, class Traits, class Alloc>
	int compare(const std::basic_string<CharT, Traits, Alloc>& left,
				const std::basic_string<CharT, Traits, Alloc>& right) const
	{
		return compare(left.c_str(), right.c_str());
	}
	template<class CharT, class Traits, class Alloc>
	int compare(const std::basic_string<CharT, Traits, Alloc>& left, const CharT* right) const
	{
		return compare(left.c_str(), right);
	}
	template<class CharT, class Traits, class Alloc>
	int compare(const CharT* left, const std::basic_string<CharT, Traits, Alloc>& right) const
	{
		return compare(left, right.c_str());
	}

	template<class StringT1, class StringT2>
	bool operator()(const StringT1& left, const StringT2& right) const
	{
		return compare(left, right) < 0;
	}
};

/**
 @brief 比较字符串的一个范围内的子串：[startPos, length)

 当使用前缀查出 ZStringTable 中一个范围[lower, upper)时，再使用该比较器在该范围内查找更小的范围
 每个字符串比较减少了 startPos 个字符的比较，因为已经知道 [0, startPos) 的字符都是相同的。
 */
class StringCompareRangeNoCase :
	public StringCompareRangeNoCaseImpl<StringCompareRangeNoCase>
{
public:
	int m_startPos;
	int m_length;

	StringCompareRangeNoCase(int startPos = 0, int length = 65535)
		: m_startPos(startPos), m_length(length) {}

private:
	friend class StringCompareRangeImpl<StringCompareRangeNoCase>;
	int getStartPos() const { return m_startPos; }
	int getLength() const { return m_length; }
};
BOOST_TT_AUX_BOOL_TRAIT_CV_SPEC1(HasTriCompare, StringCompareRangeNoCase, true)

/**
 @brief 前缀比较器

 实际上是 startPos 为 0 的 range 比较器，但是经过优化，效率更高，
 更因为 StringComparePrefix 的使用频率更高
 */
class StringComparePrefixNoCase :
	public StringCompareRangeNoCaseImpl<StringComparePrefixNoCase>
{
public:
	int m_nPrefix;

	explicit StringComparePrefixNoCase(int nPrefix = 1) : m_nPrefix(nPrefix) {}

	void setPrefix(int nPrefix) { m_nPrefix = nPrefix; }
	int  getPrefix() const { return m_nPrefix; }

	template<class CharT, class Traits, class Alloc>
	static size_t common_prefix_length(const std::basic_string<CharT, Traits, Alloc>& s1,
									   const std::basic_string<CharT, Traits, Alloc>& s2)
	{
		using namespace std;
		size_t len = 0;
		size_t size = min(s1.size(), s2.size());
		while (len < size)
		{
			if (s1[len] & 0x80)
			{
				if (s1[len] != s2[len]) return len;
				len++;

				if (s1[len] != s2[len]) return len;
			}
			else if (my_tolower(s1[len]) != my_tolower(s2[len]))
			{
				return len;
			}
			len++;
		}
		return len;
	}
private:
	friend class StringCompareRangeNoCaseImpl<StringComparePrefixNoCase>;
	int getStartPos() const { return 0; }
	int getLength() const { return m_nPrefix; }
};
BOOST_TT_AUX_BOOL_TRAIT_CV_SPEC1(HasTriCompare, StringComparePrefixNoCase, true)

class StringCompareSZ_NoCase
{
public:
	typedef StringComparePrefixNoCase prefix_compare;
	typedef StringCompareRangeNoCase   range_compare;

	int compare(const char* left, const char* right) const
	{
		const unsigned char *x = (const unsigned char*)left;
		const unsigned char *y = (const unsigned char*)right;
		while (*x && *y)
		{
		//	if (*x & 0x80 || *y & 0x80)
			if (*x & 0x80)
			{
				if (*x != *y) return *x - *y;
				x++, y++;

				if (0 == *x || 0 == *y || *x != *y)	return *x - *y;
			}
			else if (my_tolower(*x) != my_tolower(*y))
			{
				return my_tolower(*x) - my_tolower(*y);
			}
			x++, y++;
		}
		return *x - *y;
	//	return my_tolower(*x) - my_tolower(*y); // not needed
	}
	int compare(const wchar_t* left, const wchar_t* right) const
	{
#if defined(_WIN32) || defined(_WIN64)
		return _wcsicmp(left, right);
#else
		return wcscasecmp(left, right);
#endif
	}
	template<class CharT, class Traits, class Alloc>
	int compare(const std::basic_string<CharT, Traits, Alloc>& left,
				const std::basic_string<CharT, Traits, Alloc>& right) const
	{
		return compare(left.c_str(), right.c_str());
	}
	template<class CharT, class Traits, class Alloc>
	int compare(const std::basic_string<CharT, Traits, Alloc>& left, const CharT* right) const
	{
		return compare(left.c_str(), right);
	}
	template<class CharT, class Traits, class Alloc>
	int compare(const CharT* left, const std::basic_string<CharT, Traits, Alloc>& right) const
	{
		return compare(left, right.c_str());
	}

	template<class StringT1, class StringT2>
	bool operator()(const StringT1& left, const StringT2& right) const
	{
		return compare(left, right) < 0;
	}
};
BOOST_TT_AUX_BOOL_TRAIT_CV_SPEC1(HasTriCompare, StringCompareSZ_NoCase, true)

class StringCompareUrl
{
public:
	template<class CharT, class Traits, class Alloc>
	int compare(const std::basic_string<CharT, Traits, Alloc>& left,
				const std::basic_string<CharT, Traits, Alloc>& right) const
	{
		typename std::basic_string<CharT, Traits, Alloc>::const_iterator i1, e1, i2, e2;
		i1 =  left.begin(), e1 =  left.end();
		i2 = right.begin(), e2 = right.end();
		for (; i1 != e1 && i2 != e2; ++i1, ++i2)
		{
			if (my_tolower(*i1) != my_tolower(*i2))
		   	{
				return (unsigned char)my_tolower(*i1) - (unsigned char)my_tolower(*i2);
			}
		}
		if (left.size() == right.size() + 1)
		{
			if ('/' == *i1)
				// left 只比 right 多了一个结尾的 '/'，因此他们是相等的
			   	return 0;
		}
		else if (right.size() == left.size() + 1)
		{
			if ('/' == *i2)
				// right 只比 left 多了一个结尾的 '/'，因此他们是相等的
			   	return 0;
		}
		return left.size() - right.size();
	}

	template<class StringT1, class StringT2>
	bool operator()(const StringT1& left, const StringT2& right) const
	{
		return compare(left, right) < 0;
	}
};
BOOST_TT_AUX_BOOL_TRAIT_CV_SPEC1(HasTriCompare, StringCompareUrl, true)

} // namespace terark

#include "boost/type_traits/detail/bool_trait_undef.hpp"

#endif // __string_compare_hpp__


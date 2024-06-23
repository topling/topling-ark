/* vim: set tabstop=4 : */
#ifndef __terark_dict_wordseg_h__
#define __terark_dict_wordseg_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#include <assert.h>
#include <wctype.h>
#include <string>
#include <vector>
#include <iostream>

#include <boost/smart_ptr.hpp>

#include <terark/util/refcount.hpp>
#include "../const_string.hpp"
#include "gb2312_ctype.hpp"

#include "TokenWord.hpp"
/*
#if defined(_MSC_VER) && _MSC_VER > 1400
//#include <unorderedset>
//# define fast_map tr1::unordered_map
# include <map>
# define fast_map std::map
#elif defined(__GLIBCPP__) && __GLIBCPP__ >= 20030513
# include <hash_map>
# define fast_map std::hash_map
#elif defined(__GLIBCXX__) && __GLIBCXX__ >= 20040419
# include <hash_map>
# define fast_map std::hash_map
#else
# include <map>
# define fast_map std::map
#endif
*/

namespace terark { namespace wordseg {

inline
void dict_add_word(std::wstring& pool,
				   std::vector<std::pair<int,int> >& offval,
				   const_wstring str, int val)
{
	offval.push_back(std::make_pair(int(pool.size()), val));
	pool.append(str.begin(), str.end());
}

inline
const_wstring dict_get_word(std::wstring& pool,
				   std::vector<std::pair<int,int> >& offval,
				   int idx)
{
	assert(size_t(idx+1) < offval.size());
	int curroff = offval[idx+0].first;
	int nextoff = offval[idx+1].first;
	return const_wstring(&*pool.begin()+curroff, &*pool.begin()+nextoff);
}

#ifdef _MSC_VER
# pragma warning(push)
# pragma warning(disable: 4018)
#endif

//! map nth to freq
//typedef fast_map<int, int> widx_freq_map_t;
typedef std::vector<int> widx_freq_map_t;

/**
 正常的前向字典

 单词都保存在 pool 中，一个单词的长度由 index[nth+1]-index[nth] 计算出
 */
class TERARK_DLL_EXPORT DictForward : public RefCounter
{
public:
	typedef std::vector<const wchar_t*> index_t;

	DictForward(std::wstring& pool, const std::vector<std::pair<int,int> >& voffval, bool hasDupWord = true, bool isSorted=false);

	const_wstring str(size_t nth) const
	{
		assert(nth < m_index.size()-1);
		return const_wstring(m_index[nth], m_index[nth+1]);
	}

	const_wstring str(index_t::const_iterator iter) const
	{
		assert(iter >= m_index.begin());
		assert(iter <  m_index.end()-1);
		return const_wstring(*iter, *(iter+1));
	}

	int val(size_t nth) const
	{
		assert(nth < m_index.size()-1);
		return m_val[nth];
	}
	int val(index_t::const_iterator iter) const
	{
		assert(iter >= m_index.begin());
		assert(iter <  m_index.end()-1);
		return m_val[iter-m_index.begin()];
	}

	const std::vector<int>& val() const { return m_val; }
//	std::vector<int>& val() { return m_val; }

	int len(size_t nth) const
	{
		assert(nth < m_index.size()-1);
		return m_index[nth+1] - m_index[nth];
	}

	int len(index_t::const_iterator iter) const
	{
		assert(iter >= m_index.begin());
		assert(iter <  m_index.end()-1);
		return *(iter+1) - *iter;
	}

	int nth(index_t::const_iterator iter) const
	{
		assert(iter >= m_index.begin());
		assert(iter <  m_index.end());
		return iter - m_index.begin();
	}

	const index_t& index() const { return m_index; }
	int size() const { return m_val.size(); }

	int lower_bound(const_wstring word);
	int upper_bound(const_wstring word);
	std::pair<int,int>
		equal_range(const_wstring word);

protected:
	int lower_bound(index_t::const_iterator _First, index_t::const_iterator _Last, const_wstring word);
	int upper_bound(index_t::const_iterator _First, index_t::const_iterator _Last, const_wstring word);
	std::pair<int,int>
		equal_range(index_t::const_iterator _First, index_t::const_iterator _Last, const_wstring word);

protected:
	index_t          m_index;
	std::wstring     m_pool;
	std::vector<int> m_iidx;
	std::vector<int> m_val;
	int	m_minChar, m_maxChar;
};

class TERARK_DLL_EXPORT DictBackward : public RefCounter
{
public:
	typedef std::vector<const wchar_t*> index_t;

	DictBackward(const DictForward& dforward);

	const_wstring str(size_t nth) const
	{
		assert(nth < m_index.size());
		return const_wstring(m_index[nth] - m_len[nth], m_index[nth]);
	}
	const_wstring str(index_t::const_iterator iter)
	{
		assert(iter >= m_index.begin());
		assert(iter <  m_index.end());
		int len = m_len[iter-m_index.begin()];
		return const_wstring(*iter - len, *iter);
	}

	int fnth(size_t nth) const
	{
		assert(nth < m_fore_idx.size());
		return m_fore_idx[nth];
	}
	int fnth(index_t::const_iterator iter) const
	{
		assert(iter >= m_index.begin());
		assert(iter <  m_index.end());
		return m_fore_idx[iter - m_index.begin()];
	}

	int val(size_t nth) const
	{
		assert(nth < m_fore_idx.size());
		return m_fore->val(m_fore_idx[nth]);
	}
	int val(index_t::const_iterator iter) const
	{
		assert(iter >= m_index.begin());
		assert(iter <  m_index.end());
		return m_fore->val(m_fore_idx[iter - m_index.begin()]);
	}

	int len(size_t nth) const
	{
		assert(nth < m_len.size());
		return m_len[nth];
	}
	int len(index_t::const_iterator iter) const
	{
		assert(iter >= m_index.begin());
		assert(iter <  m_index.end());
		return m_len[iter - m_index.begin()];
	}

	int nth(index_t::const_iterator iter) const
	{
		assert(iter >= m_index.begin());
		assert(iter <  m_index.end());
		return iter - m_index.begin();
	}

	const index_t& index() const { return m_index; }
	int size() const { return m_fore_idx.size(); }

protected:
	index_t          m_index;
	std::vector<int> m_iidx;
	std::vector<int> m_fore_idx;
	std::vector<unsigned char> m_len;
	int m_minChar, m_maxChar;
	const DictForward* m_fore;
};

class TERARK_DLL_EXPORT DictWordSeg_Forward : public DictForward
{
	mutable long m_cBranch[5];
public:
	DictWordSeg_Forward(std::wstring& pool, const std::vector<std::pair<int,int> >& voffval, bool hasDupWord=true, bool isSorted=false)
		: DictForward(pool, voffval, hasDupWord, isSorted)
	{
		m_cBranch[0] = 0; // 识别回车
		m_cBranch[1] = 0; // 词典每词条第一字符范围之外
		m_cBranch[2] = 0; // 词典每词条第一字符无对应
		m_cBranch[3] = 0; // 只匹配词条第一字符
		m_cBranch[4] = 0; // 只匹配词条两个以上字符
	}

	std::vector<long> cBranch() const { return std::vector<long>(m_cBranch, m_cBranch + 5); }

	std::vector<TokenWord>
	seg(const const_wstring& wtext) const
	{
		return seg(wtext.begin(), wtext.end());
	}

	//! 对 [first, last) 进行切分
	std::vector<TokenWord>
	seg(const wchar_t* first, const wchar_t* last) const;
};

class TERARK_DLL_EXPORT DictWordSeg_Backward : public DictBackward
{
	mutable long m_cBranch[5];
public:
	DictWordSeg_Backward(const DictForward& dforward)
		: DictBackward(dforward)
	{
		m_cBranch[0] = 0;
		m_cBranch[1] = 0;
		m_cBranch[2] = 0;
		m_cBranch[3] = 0;
		m_cBranch[4] = 0;
	}

	std::vector<long> cBranch() const { return std::vector<long>(m_cBranch, m_cBranch + 5); }

	std::vector<TokenWord>
	seg(const const_wstring& wtext) const
	{
		return seg(wtext.begin(), wtext.end());
	}

	//! 对 [first, last) 进行切分
	std::vector<TokenWord>
	seg(const wchar_t* first, const wchar_t* last) const;
};

class TERARK_DLL_EXPORT DictWordSeg_Double : public RefCounter
{
public:
	boost::intrusive_ptr<DictWordSeg_Forward > m_fore;
	boost::intrusive_ptr<DictWordSeg_Backward> m_back;

#ifdef _MSC_VER
#  pragma warning(disable: 4355) // 'this' : used in base member initializer list
#endif
	DictWordSeg_Double(boost::intrusive_ptr<DictWordSeg_Forward> fore, boost::intrusive_ptr<DictWordSeg_Backward> back)
		: m_fore(fore), m_back(back)
	{
	}

	std::vector<TokenWord>
	seg(const const_wstring& wtext) const
	{
		return seg(wtext.begin(), wtext.end());
	}

	//! 对 [first, last) 进行切分
	std::vector<TokenWord>
	seg(const wchar_t* first, const wchar_t* last) const;

	//! forward-backward-union
	//! 对 DictWordSeg_Forward 和 DictWordSeg_Backward 的结果进行合并
	//! 在冲突的位置，取最大匹配的词
	std::vector<TokenWord>
	fb_union(const std::vector<TokenWord>& wordsFore,
		     const std::vector<TokenWord>& wordsBack
		) const;

	std::vector<TokenWord>
	simple_union(const std::vector<TokenWord>& wordsFore,
				 const std::vector<TokenWord>& wordsBack
		) const;
};

TERARK_DLL_EXPORT
DictWordSeg_Forward*
load_dict(const char* dictname, const char* sortedname, std::ostream& out = std::cout, std::ostream& = std::cerr);

#ifdef _MSC_VER
# pragma warning(pop)
#endif


} }

#endif // __terark_dict_wordseg_h__

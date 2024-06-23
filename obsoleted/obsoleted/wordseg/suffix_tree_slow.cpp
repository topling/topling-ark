#include "suffix_tree.hpp"

#include <assert.h>
#include <wctype.h>
#include <terark/util/compare.hpp>
#include "compare.hpp"
#include "search.hpp"

#include "wordseg.hpp"
#include "sort.hpp"

#include "code_convert.hpp"

#ifdef _MSC_VER
#  pragma warning(disable: 4018) // '<=' : signed/unsigned mismatch
#endif

namespace terark { namespace wordseg {

	typedef XIdentity MyGetCharCode;
	typedef StrCharPos_Searcher<wchar_t, GetCharFromBegin, MyGetCharCode> fore_search_at;
//	typedef StrCharPos_Searcher<wchar_t, GetCharFromEnd, MyGetCharCode> forward_search_at;

static fore_search_at fsearch() { return fore_search_at(); }

// use widx as area
SAME_NAME_MEMBER_COMPARATOR_EX(Compare_area_greater, uint32_t, uint32_t, .idw, std::greater<uint32_t>)

SuffixTree_WordSeg::SuffixTree_WordSeg(int min_word_len, int max_word_len)
{
	m_min_word_len = min_word_len;
	m_max_word_len = max_word_len;
}

void SuffixTree_WordSeg::prepare(const wchar_t* first, const wchar_t* last)
{
	m_text = const_wstring(first, last);
	m_tree.clear();
	m_tree.reserve(m_text.size());
	add_words(first, last);
}

void SuffixTree_WordSeg::prepare_bw(const wchar_t* first, const wchar_t* last)
{
	m_text = const_wstring(first, last);
	m_tree.clear();
	m_tree.reserve(m_text.size());
	add_words_bw(first, last);
}

void SuffixTree_WordSeg::add_words(const wchar_t* first, const wchar_t* last)
{
	for (const wchar_t* wordbeg = first; wordbeg < last;)
	{
		const wchar_t* wordend = wordbeg;
		while (wordend != last && ws_isw_asc_alnum(*wordend))
			++wordend;
		if (wordend - wordbeg >= m_min_word_len)
		{
			m_tree.push_back(const_wstring(wordbeg, wordend));
			wordbeg = wordend;
			continue;
		}
		int isdeli = 0;
		while (wordend != last && !(isdeli = ws_iswdelim(*wordend)))
			++wordend;
		if (isdeli && wordend - wordbeg < m_min_word_len)
		{
			wordbeg = wordend + 1;
			continue;
		}
		for (; wordbeg + m_min_word_len <= wordend; )
		{
 			m_tree.push_back(const_wstring(wordbeg, wordend));
			for (++wordbeg; wordbeg < wordend && ws_isw_asc_alnum(*wordbeg); ++wordbeg)
				;
		}
		wordbeg = wordend;
	}
}

void SuffixTree_WordSeg::add_words_bw(const wchar_t* first, const wchar_t* last)
{
	for (const wchar_t* wordbeg = first; wordbeg < last;)
	{
		const wchar_t* wordend = wordbeg;
		while (wordend != last && ws_isw_asc_alnum(*wordend))
			++wordend;
		if (wordend - wordbeg >= m_min_word_len)
		{
			m_tree.push_back(const_wstring(wordbeg, wordend));
			wordbeg = wordend;
			continue;
		}
		int isdeli = 0;
		while (wordend != last && !(isdeli = ws_iswdelim(*wordend)))
			++wordend;
		if (isdeli && wordend - wordbeg < m_min_word_len)
		{
			wordbeg = wordend + 1;
			continue;
		}
 		const wchar_t* p = wordbeg + m_min_word_len;
		for (;p <= wordend;)
		{
 			m_tree.push_back(const_wstring(wordbeg, p));
			for (++p ; p < wordend && ws_isw_asc_alnum(*p); ++p)
				;
		}
		assert(p-1 == wordend);
		wordbeg = wordend;
	}
}

#if defined(_DEBUG) || !defined(NDEBUG)
void SuffixTree_WordSeg::debug_check()
{
	std::vector<const_wstring>::const_iterator iter = m_tree.begin();
	for (; iter != m_tree.end(); ++iter)
	{
//		if (wcsncasecmp(L"云南大学旅游文化学院", iter->begin(), 10) == 0)
		{
			int x=0;
		}
	}
}
#endif

void SuffixTree_WordSeg::all_word_pos_loop(int minFreq, std::vector<const_wstring>& words, int pos, citer_t first, citer_t last)
{
	assert(0 == pos || towlower(first->at(pos-1)) == towlower((last-1)->at(pos-1)));
	assert(pos <= first->size());

	if (last - first < minFreq)
		return;

	if (pos >= m_min_word_len)
	{
		citer_t j = first;
		if (first->size() == pos)
		{ // 查找到第一个其尺寸大于 pos 的串的位置，就是这次搜索应该开始的位置
		  // 如果未找到这样的串，递归会在后面退出: if (first->size() == pos) return;
			for (; j != last; ++j)
			{
				if (j->size() > pos)
				{
					first = j;
					break;
				}
				words.push_back(const_wstring(j->begin(), pos));
			}
		}
		for (; j != last; ++j)
			words.push_back(const_wstring(j->begin(), pos));
	}
	if (first->size() == pos)
		return;
	if (pos >= m_max_word_len)
		return;

	while (last - first > minFreq)
	{
		citer_t iupp = fore_search_at().upper_bound(first, last, first->begin(), pos);
		all_word_pos_loop(minFreq, words, pos+1, first, iupp);
		first = iupp;
	}
}

std::vector<const_wstring>
SuffixTree_WordSeg::all_word_pos(int minFreq)
{
//	std::sort(m_tree.begin(), m_tree.end(), CompareWordsW());
	StrCharPos_Sorter<wchar_t, GetCharFromBegin, MyGetCharCode>(8*m_max_word_len).sort(m_tree.begin(), m_tree.end());

	std::vector<const_wstring> words;
	citer_t last = m_tree.end();
	all_word_pos_loop(minFreq, words, 0, m_tree.begin(), last);
	std::sort(words.begin(), words.end(), CompareBegin());

	return words;
}

void SuffixTree_WordSeg::get_word_freq_loop(int minFreq, std::vector<TokenWord>& wordsFreq, int pos, citer_t first, citer_t last)
{
	assert(0 == pos || towlower(first->at(pos-1)) == towlower((last-1)->at(pos-1)));
	assert(pos <= first->size());

	if (last - first < minFreq)
		return;

	if (pos >= m_min_word_len)
	{
		wordsFreq.push_back(TokenWord(const_wstring(first->begin(), pos), last - first, -1));
		if (first->size() == pos)
		{ // 查找到第一个其尺寸大于 pos 的串的位置，就是这次搜索应该开始的位置
		  // 如果未找到这样的串，递归会在后面退出: if (first->size() == pos) return;
			for (citer_t j = first; j != last; ++j)
			{
				if (j->size() > pos)
				{
					first = j;
					break;
				}
			}
		}
	}
	if (first->size() == pos)
		return;
	if (pos >= m_max_word_len)
		return;

	while (last - first > minFreq)
	{
		citer_t iupp = fore_search_at().upper_bound(first, last, first->begin(), pos);
		get_word_freq_loop(minFreq, wordsFreq, pos+1, first, iupp);
		first = iupp;
	}
}

std::vector<TokenWord>
SuffixTree_WordSeg::get_word_area(int minFreq)
{
//	std::sort(m_tree.begin(), m_tree.end(), CompareWordsW());
	StrCharPos_Sorter<wchar_t, GetCharFromBegin, MyGetCharCode>(8*m_max_word_len).sort(m_tree.begin(), m_tree.end());

//	debug_check();

	std::vector<TokenWord> wf;
	get_word_freq_loop(minFreq, wf, 0, m_tree.begin(), m_tree.end());
	for (std::vector<TokenWord>::iterator i = wf.begin(); wf.end() != i; ++i)
	{
		i->idw = i->area();
	}
	std::sort(wf.begin(), wf.end(), Compare_area_greater());

	return wf;
}

std::vector<TokenWord>
SuffixTree_WordSeg::get_word_freq(int minFreq)
{
	std::vector<TokenWord> wf = get_word_area(minFreq);
// 	for (std::vector<TokenWord>::iterator i = wf.begin(); wf.end() != i; ++i)
// 	{
// 		i->widx = -1;
// 	}
	return wf;
}

int FindIndex(const std::vector<TokenWord>& le, const wchar_t *szEnd)
{
	return std::lower_bound(le.begin(), le.end(), szEnd, CompareBegin()) - le.begin();
}

std::vector<int> max_subset(const std::vector<TokenWord>& le)
{
	using namespace std;
	std::vector<int> subset;
	int n = le.size()-1;
	vector<int> dpf(n+1, 0);
	vector<int> idx(n+0);
	for (int i = n-1; i >= 0; --i)
	{
		int k1 = i+1;
		int k2 = FindIndex(le, le[i].end());
		int f0 = dpf[k1];
		int f1 = dpf[k2] + le[i].freq;
		if (f1 > f0)
			idx[i] = k2, dpf[i] = f1;
		else
			idx[i] = -1, dpf[i] = f0;
	}
	for (int i = 0; i < n;)
	{
		if (idx[i] >= 0) {
			subset.push_back(i);
			assert(idx[i] > i);
			i = idx[i];
		} else
			++i;
	}
	return subset;
}

class GetFirstStringCharAtPos
{
public:
	wchar_t operator()(const std::pair<const_wstring, int>& x, int pos) const
	{
		return x.first[pos];
	}
};

/*
// 简洁版
std::vector<TokenWord> SuffixTree_WordSeg::forward_word_pos(int minFreq)
{
	using namespace std;

	typedef vector<TokenWord>::const_iterator wf_iter_t;

	vector<const_wstring> words0 = m_tree, words1;

//	std::sort(m_tree.begin(), m_tree.end(), CompareWordsW());
	StrCharPos_Sorter<wchar_t, GetCharFromBegin, GetCharCode>(8*m_max_word_len).sort(m_tree.begin(), m_tree.end());

	vector<TokenWord> wf;
	get_word_freq_loop(minFreq, wf, 0, m_tree.begin(), m_tree.end());
	for (vector<TokenWord>::iterator i = wf.begin();
		 wf.end() != i; ++i)
	{
		i->second *= i->size();
	}

 	vector<pair<wf_iter_t,int> > window; window.reserve(m_max_word_len);
	for (citer_t iter = words0.begin(); words0.end() != iter;)
	{
#if defined(_DEBUG) || !defined(NDEBUG)
		int nth = iter - words0.begin();
		assert(words0.end()-1 == iter || iter->begin() < iter[1].begin());
#endif
		window.clear();
		int pos = 0;
		pair<wf_iter_t,wf_iter_t> ii, jj(wf.begin(), wf.end());
		do {
			ii = jj;
			jj = StrCharPos_Compare(pos).equal_range(jj.first, jj.second, iter->begin(), GetFirstStringCharAtPos());
			pos++;
			if (pos == jj.first->size())
			{
				assert(pos >= m_min_word_len);
				window.push_back(make_pair(ii.first, ii.first->second));
				++jj.first;
			}
		} while (jj.second != jj.first && pos <= ii.first->size());
		pos--;

#if defined(_DEBUG) || !defined(NDEBUG)
		int range_size = ii.second - ii.first;
#endif
		if (pos <= m_min_word_len)
		{
			++iter;
			continue;
		}
		std::sort(window.begin(), window.end(), CompareSecond<int>());
		const_wstring best_word = window.back().first->first;
		const_wstring iter_word(iter->begin(), best_word.size());
		words1.push_back(iter_word);
		while (words0.end() != iter && iter->begin() < iter_word.end())
			++iter;
	}
	return words1;
}
*/

// 优化版
//! 先调用 get_word_freq_loop 得到高频词，再对文章进行第二遍扫描，使用这些高频词作为字典
std::vector<TokenWord> SuffixTree_WordSeg::forward_word_pos(int minFreq)
{
	using namespace std;

	typedef vector<TokenWord>::const_iterator wf_iter_t;

	vector<const_wstring> words0 = m_tree;
	vector<TokenWord> words1;

//	std::sort(m_tree.begin(), m_tree.end(), CompareWordsW());
	StrCharPos_Sorter<wchar_t, GetCharFromBegin, MyGetCharCode>(8*m_max_word_len).sort(m_tree.begin(), m_tree.end());

#define idw_as_area widx
	vector<TokenWord> wf;
	get_word_freq_loop(minFreq, wf, 0, m_tree.begin(), m_tree.end());
	for (vector<TokenWord>::iterator i = wf.begin(); wf.end() != i; ++i)
	{
		i->idw_as_area = i->freq * i->size();
	}

 	vector<pair<wf_iter_t,int> > window; window.reserve(m_max_word_len);
	for (citer_t iter = words0.begin(); words0.end() != iter;)
	{
#if defined(_DEBUG) || !defined(NDEBUG)
		int nth = iter - words0.begin();
		assert(words0.end()-1 == iter || iter->begin() < iter[1].begin());
#endif
		window.clear();
		int pos = 0;
		pair<wf_iter_t,wf_iter_t> ii(wf.begin(), wf.end());
		do {
			ii = fore_search_at().equal_range(ii.first, ii.second, iter->begin(), pos);
			pos++;
			if (ii.first == ii.second)
				break;
			if (pos == ii.first->size())
			{
				assert(pos >= m_min_word_len);
				window.push_back(make_pair(ii.first, ii.first->idw_as_area));
				++ii.first;
			}
		} while (ii.second - ii.first > 1 && pos < ii.first->size());
#if defined(_DEBUG) || !defined(NDEBUG)
		int range_size = ii.second - ii.first;
#endif
		if (pos <= m_min_word_len)
		{
			++iter;
			continue;
		}
		int max_prefix = ii.first->size();
		if (ii.second - ii.first == 1 &&
			fore_search_at().prefix(*ii.first, *iter, pos, max_prefix) == max_prefix)
		{
			window.push_back(make_pair(ii.first, ii.first->idw_as_area));
		}
		std::sort(window.begin(), window.end(), CompareSecond<int>());
		TokenWord     best_word = *window.back().first;
		const_wstring iter_word(iter->begin(), best_word.size());
		words1.push_back(TokenWord(iter_word, best_word.freq, -1));
		while (words0.end() != iter && iter->begin() < iter_word.end())
			++iter;
	}
	return words1;
}

//! 直接对后缀树数组进行扫描，使用最大前向匹配，这个“最大”，指的是 word.size()*freq 最大
std::vector<TokenWord> SuffixTree_WordSeg::forward_word_pos2(int minFreq)
{
	using namespace std;

	vector<TokenWord> words1;

	if (m_tree.empty())
		return words1;

//	std::sort(m_tree.begin(), m_tree.end(), CompareWordsW());
	StrCharPos_Sorter<wchar_t, GetCharFromBegin, MyGetCharCode>
		(8*m_max_word_len).sort(m_tree.begin(), m_tree.end());
	typedef std::map<const_wstring, int, CompareWordsW> tw_map_t;
	tw_map_t tw_map;
	int line = 0;
	const wchar_t* lineBeg = m_text.begin();
	for (const wchar_t* iter = m_text.begin(); iter < m_text.end();)
	{
		if (*iter == L'\n')
		{
			++line;
			++iter;
			lineBeg = iter;
			continue;
		}
		const_wstring iter_word;
		int bIsAllEngAlphaNum = -1;
		int pos = 0;
		int area1 = 0, freq1 = m_tree.size();
		pair<citer_t,citer_t> ii(m_tree.begin(), m_tree.end());
		do {
			bIsAllEngAlphaNum &= ws_isw_asc_alnum(iter[pos]);
			ii = fore_search_at().equal_range(ii.first, ii.second, iter, pos);
			pos++;
			if (ii.first == ii.second)
				break;
			if (pos >= m_min_word_len)
			{
				assert(ii.first->size() >= pos);
				int freq2 = ii.second - ii.first;
				if (bIsAllEngAlphaNum)
				{
					iter_word = const_wstring(iter, pos);
					area1 = pos * freq2;
					freq1 = freq2;
				}
				else if (freq2 >= minFreq)
				{
					int area2 = pos * freq2;
					if (area2 >= area1) // long word is preferred
					{
						iter_word = const_wstring(iter, pos);
						area1 = area2;
						freq1 = freq2;
					}
				} else
					break;
				assert(ii.second > ii.first);
				while (ii.first->size() == pos)
				{
					if (++ii.first == ii.second)
						goto ExtExit;
				}
			}
		} while ((bIsAllEngAlphaNum || ii.second - ii.first >= minFreq)
				 && pos < ii.first->size()
				 && pos < (bIsAllEngAlphaNum ? 8*m_max_word_len:m_max_word_len)
				 );
ExtExit:
#if defined(_DEBUG) || !defined(NDEBUG)
		int range_size = ii.second - ii.first;
#endif
#if 0
#define PRINT_ILL_WORD(lead, delim) printf(lead "_word=%s" delim, wcs2str(iter_word).c_str())
#else
#define PRINT_ILL_WORD(lead, delim)
#endif
		if (iter_word.empty())
		{
			++iter;
			continue;
		}

		if (!bIsAllEngAlphaNum && pos > m_max_word_len)
		{ // 英文单词后面有个汉字
			PRINT_ILL_WORD("fh", ", ");
			const wchar_t* p = iter_word.end() - 1;
			while (p >= iter_word.begin() + m_max_word_len && !ws_isw_asc_alnum(*p))
				--p;
			iter_word = const_wstring(iter_word.begin(), p+1);
			PRINT_ILL_WORD("fh", "\n");
		}
		else if (!bIsAllEngAlphaNum &&
				ws_isw_asc_alnum(iter_word.end()[-1]) &&
				ws_isw_asc_alnum(iter_word.end()[0]))
		{ // 边界不能为字母数字
			PRINT_ILL_WORD("fe", ", ");
			const wchar_t* p = iter_word.end() - 2;
			while (p >= iter_word.begin() && ws_isw_asc_alnum(*p))
				--p;
			if (p - iter_word.begin() >= m_min_word_len)
				iter_word = const_wstring(iter_word.begin(), p+1);
			PRINT_ILL_WORD("fe", "\n");
		}
		TokenWord tw(iter_word, freq1, -1, tw_map[iter_word]++);
		tw.line = line;
		tw.colu = iter_word.begin() - lineBeg;
		words1.push_back(tw);
		iter = iter_word.end();
	}
	for (std::vector<TokenWord>::iterator i = words1.begin(); i != words1.end(); ++i)
	{
		i->freq = tw_map[*i];
	}
	return words1;
}

//! 直接对后缀树数组进行扫描，使用最大逆向匹配，这个“最大”，指的是 word.size()*freq 最大
std::vector<TokenWord> SuffixTree_WordSeg::backward_word_pos(int minFreq)
{
	using namespace std;
	typedef StrCharPos_Searcher<wchar_t, GetCharFromEnd, MyGetCharCode> back_search_at;

	vector<TokenWord> words1;
	if (m_tree.empty())
		return words1;

//	std::sort(m_tree.begin(), m_tree.end(), CompareWordsBackward());
	StrCharPos_Sorter<wchar_t, GetCharFromEnd_ForSort, MyGetCharCode>
		(8*m_max_word_len).sort(m_tree.begin(), m_tree.end());

//	double log_2 = ::log(2.0);
	typedef std::map<const_wstring, int, CompareWordsW> tw_map_t;
	tw_map_t tw_map;
 	int line = 0;
	const wchar_t* lineEnd = m_text.end();
	for (const wchar_t* iter = m_text.end(); iter > m_text.begin(); )
	{
		if (iter[-1] == L'\n')
		{
			if (!words1.empty())
			{
				int max_colu = lineEnd - iter;
				vector<TokenWord>::iterator j = words1.end();
				for (;;) {
					--j;
					if (j->line == line)
						j->colu = max_colu - j->colu;
					else
						break;
					if (words1.begin() == j)
						break;
				}
			}
			lineEnd = iter;
			++line;
			--iter;
			continue;
		}
		const_wstring iter_word;
		int bIsAllEngAlphaNum = -1;
		int pos = 0;
		int area1 = 0, freq1 = m_tree.size();
		pair<citer_t,citer_t> ii(m_tree.begin(), m_tree.end());
		do {
			pos--;
			bIsAllEngAlphaNum &= ws_isw_asc_alnum(iter[pos]);
			ii = back_search_at().equal_range(ii.first, ii.second, iter, pos);
			if (ii.first == ii.second)
				break;
			if (-pos >= m_min_word_len)
			{
				assert(ii.first->size() >= -pos);
				int freq2 = ii.second - ii.first;
				if (bIsAllEngAlphaNum)
				{
					iter_word = const_wstring(iter+pos, iter);
					area1 = -pos * freq2;
					freq1 = freq2;
				}
				else if (freq2 >= minFreq)
				{
					int area2 = -pos * freq2;
					if (area1 <= area2)
					{
						iter_word = const_wstring(iter+pos, iter);
						area1 = area2;
						freq1 = freq2;
					}
				} else
					break;
				assert(ii.second > ii.first);
				while (ii.first->size() == -pos)
				{
					if (++ii.first == ii.second)
						goto ExtExit;
				}
			}
		} while ((bIsAllEngAlphaNum || ii.second - ii.first >= minFreq)
				 && -pos < ii.first->size()
				 && -pos < (bIsAllEngAlphaNum ? 8*m_max_word_len:m_max_word_len)
				 );
ExtExit:
#if defined(_DEBUG) || !defined(NDEBUG)
		int range_size = ii.second - ii.first;
#endif
		if (iter_word.empty())
		{
			--iter;
			continue;
		}

		if (!bIsAllEngAlphaNum && -pos > m_max_word_len)
		{ // 英文单词前面有个汉字
			PRINT_ILL_WORD("bh", ", ");
			const wchar_t* p = iter_word.begin();
			while (p < iter_word.end() && !ws_isw_asc_alnum(*p))
				++p;
			iter_word = const_wstring(p, iter_word.end());
			PRINT_ILL_WORD("bh", "\n");
		}
		else if (!bIsAllEngAlphaNum &&
				ws_isw_asc_alnum(iter_word[0]) &&
				iter_word.begin() > m_text.begin() &&
				ws_isw_asc_alnum(iter_word.begin()[-1]))
		{ // 边界不能为字母数字
			PRINT_ILL_WORD("be", ", ");
			const wchar_t* p = iter_word.begin()+1;
			while (p < iter_word.end() && ws_isw_asc_alnum(*p))
				++p;
			if (iter_word.end() - p >= m_min_word_len)
				iter_word = const_wstring(p, iter_word.end());
			PRINT_ILL_WORD("be", "\n");
		}
		TokenWord tw(iter_word, freq1, -1, tw_map[iter_word]++);
 		tw.line = line;
		tw.colu = lineEnd - iter_word.begin();
		words1.push_back(tw);
		iter = iter_word.begin();
	}
	if (!words1.empty() && L'\n' != *m_text.begin())
	{
		int max_colu = lineEnd - m_text.begin() - 1;
		vector<TokenWord>::iterator j = words1.end();
		for (;;) {
			--j;
			if (j->line == line)
				j->colu = max_colu - j->colu;
			else
				break;
			if (words1.begin() == j)
				break;
		}
	}
	for (std::vector<TokenWord>::iterator i = words1.begin(); i != words1.end(); ++i)
	{
		i->freq = tw_map[*i];
		i->ntho = i->freq - i->ntho - 1;
		i->line = line - i->line;
	}
	std::reverse(words1.begin(), words1.end());
	return words1;
}

//! 使用“最大覆盖子集”的算法确定所选的词
//!
//! weight = word.size()*freq, 对窗口内的每个 [pos, pos+size) 进行统计
//! 其中 pos 从窗口起点到终点， size 也是从起点到终点，因此有 (max-min)*(max-min+1)/2 个词
std::vector<TokenWord> SuffixTree_WordSeg::forward_word_pos3(int minFreq)
{
	using namespace std;

	std::vector<TokenWord> words1;
	if (m_tree.empty())
		return words1;

	vector<const_wstring> words0 = m_tree;

//	std::sort(m_tree.begin(), m_tree.end(), CompareWordsW());
	StrCharPos_Sorter<wchar_t, GetCharFromBegin, MyGetCharCode>(8*m_max_word_len).sort(m_tree.begin(), m_tree.end());
//	double log_2 = ::log(2.0);

 	vector<TokenWord> window; window.reserve(m_max_word_len*m_max_word_len);
	for (citer_t iter = words0.begin(); words0.end() != iter;)
	{
#if defined(_DEBUG) || !defined(NDEBUG)
		int nth = iter - words0.begin();
		assert(words0.end()-1 == iter || iter->begin() < iter[1].begin());
#endif
		window.clear();
		for (const wchar_t* wend = iter->end(); words0.end() != iter && iter->begin() <= wend; ++iter)
		{
			int pos = 0;
			pair<citer_t,citer_t> ii(m_tree.begin(), m_tree.end());
			do {
				int freq0 = ii.second - ii.first;
				ii = fore_search_at().equal_range(ii.first, ii.second, iter->begin(), pos);
				pos++;
				if (pos >= m_min_word_len)
				{
					assert(ii.first->size() >= pos);
					int freq1 = ii.second - ii.first;
					if (freq1 >= minFreq) {
						const_wstring word(iter->begin(), pos);
					//	double idf = ::log(double(m_text.size()*pos)/freq) / log_2;
					//	int weight = int(idf * freq * pos);
						int weight = pos * m_text.size() * freq1 / freq0;
						window.push_back(TokenWord(word, freq1, weight));
					} else
						break;
					while (ii.first != ii.second && ii.first->size() == pos)
						++ii.first;
				}
			} while (ii.second - ii.first > minFreq &&
					pos < ii.first->size() &&
					pos < m_max_word_len &&
					pos < iter->size());
#if defined(_DEBUG) || !defined(NDEBUG)
			int range_size = ii.second - ii.first;
#endif
		}
		window.push_back(TokenWord(const_wstring(m_text.end(),m_text.end()), 0, -1));
		std::vector<int> subset = max_subset(window);
		for (int j = 0; j != subset.size(); ++j)
		{
			words1.push_back(window[subset[j]]);
		}
	}
	return words1;
}

/**
 @brief
 @note
  该函数完成后，m_tree 是 forward tree
 */
std::vector<TokenWord>
SuffixTree_WordSeg::bidirect_word_pos(const const_wstring text, int minFreq)
{
	std::vector<TokenWord> words;
//	assert(!text.empty());
	if (text.empty())
		return words;

#if 0 // print or not print
#define PrintConflict()  \
	{ \
		int f1 = i->freq; \
		int f2 = j->freq; \
		string wi = wcs2str(*i), wj = wcs2str(*j); \
		printf("conflict: f=(%s,%d), b=(%s,%d)\n", wi.c_str(), f1, wj.c_str(), f2); \
	}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#else
#define PrintConflict()
#endif

	prepare_bw(text);
	std::vector<TokenWord> words_j = backward_word_pos(minFreq);

	prepare(text);
	std::vector<TokenWord> words_i = forward_word_pos2(minFreq);

	words.reserve(words_i.size()+words_j.size());

	std::vector<TokenWord>::const_iterator i = words_i.begin(), j = words_j.begin();
	while (i != words_i.end() && j != words_j.end())
	{
// 		if (wcsncmp(i->data(), L"系列为", 3) == 0)
// 		{
// 			int aaa = 0;
// 		}
// 		if (wcsncmp(j->data(), L"系列为", 3) == 0)
// 		{
// 			int aaa = 0;
// 		}
		if (i->begin() < j->begin())
		{
			PrintConflict();
			words.push_back(*i);
			++i;
		}
		else if (j->begin() < i->begin())
		{
			PrintConflict();
			words.push_back(*j);
			++j;
		}
		else if (i->end() == j->end())
		{
			words.push_back(*i);
			++i; ++j;
		}
		else // conflict, begin() is equal but end() is not equal
		{
			PrintConflict();
			int area1 = i->area();
			int area2 = j->area();
			if (area1 > area2) {
				words.push_back(*i);
			} else {
				words.push_back(*j);
			}
			++i; ++j;
		}
	}
	for (; words_i.end() != i; ++i)
	{
		words.push_back(*i);
	}
	for (; words_j.end() != j; ++j)
	{
		words.push_back(*j);
	}
	return words;
}


} } // namespace terark::wordseg

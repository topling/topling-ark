#ifndef __terark_suffix_algo_h__
#define __terark_suffix_algo_h__

#include "TokenWord.hpp"
#include "compare_at.hpp"
#include <terark/c/algorithm.h>
#include <terark/gold_hash_map.hpp>

#include <stdio.h>
#include <string.h>
#include <ctype.h>

namespace terark { namespace wordseg {

template<class CharT>
struct SuffixTreeAlg
{
	typedef basic_const_string<CharT> cs_t;

	int minlen;
	int maxlen;
	int minfreq;
	int ntree;
	int ntext; // ntext >= ntree
	const int* tree;
	const CharT* text;

	SuffixTreeAlg()
	{
		minlen = 2;
		maxlen = 20;
		minfreq = 2;
		ntree = 0;
		ntext = 0;
		tree = NULL;
		text = NULL;
	}

	static bool eng_isalnum(CharT c)
	{
		return 0 < c&&c < 128 && isalnum(c);
	}

	struct Hash_cs
	{
		size_t operator()(const cs_t& cs) const
		{
			size_t h = 1089759;
			for (ptrdiff_t i = 0, n = cs.size(); i < n; ++i)
				h = ( (h >> (sizeof(size_t)-3) | h << 3) | cs[i] ) + 37981 ;
			return h;
		}
		// equal
		bool operator()(const cs_t& x, const cs_t& y) const
		{
			if (x.size() != y.size()) return false;
			return memcmp(x.begin(), y.begin(), x.size() * sizeof(CharT)) == 0;
		}
	};
	typedef gold_hash_map<cs_t, ptrdiff_t, Hash_cs, Hash_cs> tw_map_t;

	//! 直接对后缀树数组进行扫描，使用最大前向匹配，这个“最大”，指的是 word.size()*freq 最大

	std::vector<TokenWord_CT<CharT> > forward_word_pos2() const
	{
		using namespace std;

		assert(minfreq >= 1);
		assert(minlen >= 1);
		assert(maxlen >= 2);
		assert(ntree > 0);
		assert(ntext > 0);
		assert(ntree <= ntext);

		vector<TokenWord_CT<CharT> > words1;

		if (0 == ntree)
			return words1;

		tw_map_t tw_map;

		int line = 0;
		const CharT* lineBeg = text;
		const CharT* tEnd = text + ntext;
		CompareAt<CharT> comp;
		comp.len = ntext;
		comp.str = text;

		for (const CharT* iter = text; iter < tEnd;)
		{
			if (*iter == '\n')
			{
				++line;
				++iter;
				lineBeg = iter;
				continue;
			}
			cs_t iter_word;
			int bIsAllEngAlphaNum = -1;
			int area1 = 0, freq1 = ntree;
			comp.pos = 0;
			pair<const int*, const int*> ii(tree, tree + ntree), ii2;
			do {
				assert(ii.first [ 0] + comp.pos < ntext);
				assert(ii.second[-1] + comp.pos < ntext);
				bIsAllEngAlphaNum &= eng_isalnum(iter[comp.pos]);
				ii = std::equal_range(ii.first, ii.second, ChKey<CharT>(iter[comp.pos]), comp);
				if (ii.first == ii.second)
					break;
				comp.pos++;
				if (comp.pos >= minlen)
				{
					assert(*ii.first + comp.pos <= ntext);
					int freq2 = ii.second - ii.first;
					if (bIsAllEngAlphaNum)
					{
						iter_word = cs_t(iter, comp.pos);
						area1 = comp.pos * freq2;
						freq1 = freq2;
						ii2 = ii;
					}
					else if (freq2 >= minfreq)
					{
						int area2 = comp.pos * freq2;
						if (area2 >= area1) // long word is preferred
						{
							iter_word = cs_t(iter, comp.pos);
							area1 = area2;
							freq1 = freq2;
							ii2 = ii;
						}
					} else
						break;
					assert(ii.second > ii.first);
				}
				while (*ii.first + comp.pos == ntext) ++ii.first;
			} while ((bIsAllEngAlphaNum || ii.second - ii.first >= minfreq)	&& *ii.first + comp.pos < ntext && iter + comp.pos < tEnd);
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
#if 0
			if (!bIsAllEngAlphaNum && comp.pos > maxlen)
			{ // 英文单词后面有个汉字
				PRINT_ILL_WORD("fh", ", ");
				const CharT* p = iter_word.end() - 1;
				while (p >= iter_word.begin() + maxlen && !eng_isalnum(*p))
					--p;
				iter_word = cs_t(iter_word.begin(), p+1);
				PRINT_ILL_WORD("fh", "\n");
			}
			else if (!bIsAllEngAlphaNum &&
					eng_isalnum(iter_word.end()[-1]) &&
					eng_isalnum(iter_word.end()[0]))
			{ // 边界不能为字母数字
				PRINT_ILL_WORD("fe", ", ");
				const CharT* p = iter_word.end() - 2;
				while (p >= iter_word.begin() && eng_isalnum(*p))
					--p;
				if (p - iter_word.begin() >= minlen)
					iter_word = cs_t(iter_word.begin(), p+1);
				PRINT_ILL_WORD("fe", "\n");
			}
#endif
//			if (iter_word.size() == comp.pos)
			if (freq1 > 1)
		   	{
				// 看是否是 tandem repeat, 即连续重复的串
				assert(ii2.second - ii2.first == freq1);
//				assert(freq1 > 1);
				int vmin = *ii2.first;
				int vmax = *ii2.first;
				const int* p;
				for (p = ii2.first + 1; p < ii2.second; ++p) {
					if (*p < vmin) vmin = *p;
					if (*p > vmax) vmax = *p;
				}
				int len = iter_word.size();
				int rem = (vmax - vmin) % (freq1 - 1);
				fprintf(stderr, "iter=%d min=%u max=%u freq=%d rem=%d len=%d\n", iter - text, vmin, vmax, freq1, rem, len);
				if (0 == rem && iter - text == vmin)
			   	{ // 判断是否等差数列（可能被打乱了的等差数列）
					int d = (vmax - vmin) / (freq1 - 1);
					if (d <= iter_word.size() && len % d == 0) {
						fprintf(stderr, "potential tandem\n");
						for (p = ii2.first; p != ii2.second; ++p) {
							if ((*p - vmin) % d != 0)
								goto Normal;
						}
						// 是等差数列
						assert(iter_word.begin() == iter);
						int  n = (vmax + len - vmin) / d;
						int &ref = tw_map[cs_t(iter, d)];
						for (int i = 0; i < n; ++i, iter += d) {
							TokenWord tw(cs_t(iter, d), n, -1, ref + i);
							tw.line = line;
							tw.colu = tw.begin() - lineBeg;
							words1.push_back(tw);
						}
						ref += n;
						fprintf(stderr, "found tandem:[d=%d n=%d od=%d vmax_end=%d iter=%d]\n", d, n, comp.pos, vmax + len, iter - text);
						assert(text + vmax + len == iter);
						continue;
					}
				}
			}
		Normal:
			TokenWord tw(iter_word, freq1, -1, tw_map[iter_word]++);
			tw.line = line;
			tw.colu = iter_word.begin() - lineBeg;
			words1.push_back(tw);
			iter = iter_word.end();
		}
		for (typename std::vector<TokenWord_CT<CharT> >::iterator i = words1.begin(); i != words1.end(); ++i)
		{
			// 因为分词扫描过程中一个后缀树中的词可能被分到的词覆盖，
			// 所以 i->freq 必然大于后缀树中该词的 equal_range
			//
			assert(i->freq >= tw_map[*i]);
			i->idw = tw_map[*i];
		}
		return words1;
	}

	void sortby_word_area(std::vector<TokenWord_CT<CharT> >& words) const
	{
		// .idw as area
		for (int i = 0, n = words.size(); i != n; ++i) words[i].idw = words[i].area();
		terark_sort_field(&words[0], words.size(), idw);
		std::reverse(words.begin(), words.end());
	}

	template<class Filter>
	void get_word_freq(std::vector<TokenWord_CT<CharT> >& wordsFreq, const Filter& filter) const
	{
		get_word_freq_loop(0, tree, tree + ntree, wordsFreq, filter);
	}

protected:
	template<class Filter>
	void get_word_freq_loop(int pos, const int* first, const int* last, std::vector<TokenWord_CT<CharT> >& wordsFreq, const Filter& filter) const
	{
		if (last - first < minfreq)
			return;

		if (pos >= minlen)
		{
			int slen = pos;
			int freq = last - first;
			int istr = *first;
			const CharT* sstr = text + istr;
			if (filter(sstr, slen, freq)) {
				if (!wordsFreq.empty() && wordsFreq.back().begin() == sstr && wordsFreq.back().freq == freq)
					wordsFreq.back().word() = cs_t(text + istr, slen);
				else
					wordsFreq.push_back(TokenWord_CT<CharT>(cs_t(text + istr, slen), freq, -1));
			}
			if (*first + pos == ntext)
			{ // 查找到第一个其尺寸大于 pos 的串的位置，就是这次搜索应该开始的位置
				// 如果未找到这样的串，递归会在后面退出: if (first->size() == pos) return;
				for (const int *j = first; j != last; ++j)
				{
					if (*j + pos > ntext)
					{
						first = j;
						break;
					}
				}
			}
		}
		if (*first + pos == ntext)
			return;
		if (pos >= maxlen)
			return;

		CompareAt<CharT> comp;
		comp.pos = pos;
		comp.len = ntext;
		comp.str = text;
		while (last - first > minfreq)
		{
			const int* iupp = std::upper_bound(first, last, ChKey<CharT>(text[*first + pos]), comp);
			get_word_freq_loop(pos + 1, first, iupp, wordsFreq, filter);
			first = iupp;
		}
	}

};

} } // namespace terark::wordseg

#endif // __terark_suffix_algo_h__


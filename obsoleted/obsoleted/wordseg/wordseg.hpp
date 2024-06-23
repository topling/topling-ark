/* vim: set tabstop=4 : */
#ifndef __terark_wordseg_h__
#define __terark_wordseg_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#if defined(_WIN32) || defined(_WIN64)
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif
#endif

#include <assert.h>
#include <wctype.h>
#include <string>
#include <vector>
#include <map>
#include "../const_string.hpp"

#include <terark/util/compare.hpp>
#include "compare.hpp"
#include "dict_wordseg.hpp"
#include "suffix_tree.hpp"

namespace terark { namespace wordseg {

template<class OutIter>
OutIter word_any_to_freq(const std::vector<std::pair<const_wstring,int> >& words, OutIter dest, CompareWordsW comp = wcsncasecmp)
{
	using namespace std;

	vector<pair<const_wstring,int> >::const_iterator iter;
	for (iter = words.begin(); iter != words.end();)
	{
		int freq = 0;
		const_wstring w = iter->first;
		do {
			++freq;
			++iter;
		} while (iter != words.end() && comp.compare(w, iter->first) == 0);
		*dest++ = make_pair(w, freq);
	}
//	words.erase(iter1, words.end());
	return dest;
}

template<class OutIter>
OutIter word_any_to_freq(const std::vector<TokenWord>& words, OutIter dest, CompareWordsW comp = wcsncasecmp)
{
	using namespace std;

	vector<TokenWord>::const_iterator iter;
	for (iter = words.begin(); iter != words.end();)
	{
		TokenWord tw(*iter);
		tw.freq = 0;
		do {
			++tw.freq;
			++iter;
		} while (iter != words.end() && comp.compare(tw, *iter) == 0);
		*dest++ = tw;
	}
	return dest;
}

template<class OutIter>
OutIter word_any_to_freq(const std::vector<const_wstring>& words, OutIter dest, CompareWordsW comp = wcsncasecmp)
{
	using namespace std;

	vector<const_wstring>::const_iterator iter;
	for (iter = words.begin(); iter != words.end();)
	{
		int freq = 0;
		const_wstring w(*iter);
		do {
			++freq;
			++iter;
		} while (iter != words.end() && comp.compare(w, *iter) == 0);
		*dest++ = make_pair(w, freq);
	}
	return dest;
}

template<class FwIter, class Compare>
FwIter merge_word_freq(FwIter first, FwIter last, Compare comp)
{
	std::sort(first, last, CompareAnyFirst<Compare>(comp));

	FwIter iter0 = first;
	for (FwIter iter = first; iter != last;)
	{
		int freq = 0;
		typename std::iterator_traits<FwIter>::value_type::first_type word = iter->first;
		do {
			freq += iter->second;
			++iter;
		} while (iter != last && !comp(word, iter->first));

		*iter0 = std::make_pair(word, freq);  ++iter0;
	}
	return iter0;
}

template<class TW, class Comp>
double words_cos(const std::vector<TW>& words1,
				 const std::vector<TW>& words2, Comp comp);

template<class TW, class Comp>
double words_cos_min(const std::vector<TW>& words1,
					 const std::vector<TW>& words2, Comp comp);


} }

#endif // __terark_wordseg_h__

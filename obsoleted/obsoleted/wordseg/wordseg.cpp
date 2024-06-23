#include "wordseg.hpp"
#include "code_convert.hpp"
#include <math.h>

namespace terark { namespace wordseg {

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template<class TW, class Comp>
double words_cos(const std::vector<TW>& words1,
				 const std::vector<TW>& words2, Comp comp)
{
	typedef typename std::vector<TW>::const_iterator iter_t;

	double a = 0.0, b = 0.0, c = 0.0;
	iter_t i = words1.begin(), j = words2.begin();

	while (i != words1.end() && j != words2.end())
	{
		int icomp = comp.compare(*i, *j);
		int v1 = i->freq;
		int v2 = j->freq;
		if (icomp < 0)
		{
			a += v1*v1;
			++i;
		}
		else if (icomp > 0)
		{
			b += v2*v2;
			++j;
		}
		else
		{
			a += v1*v1;
			b += v2*v2;
			c += v1*v2;
			++i; ++j;
		}
	}
	for (; i != words1.end(); ++i)
		a += i->freq * i->freq;
	for (; j != words2.end(); ++j)
		b += j->freq * j->freq;

	double d = a * b;
	double res = d < 0.0000001 ? 0.0 : c / sqrt(d);
	return res;
}

template<class TW, class Comp>
double words_cos_min_imp(const std::vector<TW>& words1,
						 const std::vector<TW>& words2, Comp comp)
{
	assert(words1.size() < words2.size());

	typedef typename std::vector<TW>::const_iterator iter_t;

	double a = 0.0, b = 0.0, c = 0.0;
	iter_t i = words1.begin(), j = words2.begin();

	while (i != words1.end() && j != words2.end())
	{
		int icomp = comp.compare(*i, *j);
		int v1 = i->freq;
		int v2 = j->freq;
		if (icomp < 0)
		{
			a += v1*v1;
			++i;
		}
		else if (icomp > 0)
		{
		//	b += v2*v2;
			++j;
		}
		else
		{
			a += v1*v1;
			b += v2*v2;
			c += v1*v2;
			++i; ++j;
		}
	}
	for (; i != words1.end(); ++i)
		a += i->freq * i->freq;
// 	for (; j != words2.end(); ++j)
// 		b += j->second * j->second;

	double d = a * b;
	double res = d < 0.0000001 ? 0.0 : c / sqrt(d);
	return res;
}

template<class TW, class Comp>
double words_cos_min(const std::vector<TW>& words1,
					 const std::vector<TW>& words2, Comp comp)
{
	if (words1.size() < words2.size())
		return words_cos_min_imp(words1, words2, comp);
	else if (words1.size() > words2.size())
		return words_cos_min_imp(words2, words1, comp);
	else
		return words_cos(words1, words2, comp);
}

//template double words_cos_min<TokenWord_CA, CompareWordsA>(const std::vector<TokenWord_CA>& words1, const std::vector<TokenWord_CA>& words2, CompareWordsA comp);
// template double words_cos_min<TokenWord_CW, CompareWordsW>;
// template double words_cos_min<TokenWord_SA, CompareWordsA>;
// template double words_cos_min<TokenWord_SW, CompareWordsW>;
//
// template double words_cos<TokenWord_CA, CompareWordsA>;
// template double words_cos<TokenWord_CW, CompareWordsW>;
// template double words_cos<TokenWord_SA, CompareWordsA>;
// template double words_cos<TokenWord_SW, CompareWordsW>;

TERARK_DLL_EXPORT
void instantial_template_words_cos()
{
	std::vector<TokenWord_CA> CA1, CA2;
	std::vector<TokenWord_CW> CW1, CW2;
	std::vector<TokenWord_SA> SA1, SA2;
	std::vector<TokenWord_SW> SW1, SW2;

	words_cos_min(CA1, CA2, CompareWordsA());
	words_cos_min(CW1, CW2, CompareWordsW());
	words_cos_min(SA1, SA2, CompareWordsA());
	words_cos_min(SW1, SW2, CompareWordsW());

	words_cos(CA1, CA2, CompareWordsA());
	words_cos(CW1, CW2, CompareWordsW());
	words_cos(SA1, SA2, CompareWordsA());
	words_cos(SW1, SW2, CompareWordsW());
}

} } // namespace terark::wordseg

#include "dict_wordseg.hpp"
#include <terark/util/compare.hpp>
#include <terark/set_op.hpp>
#include "compare.hpp"
#include "search.hpp"
#include "code_convert.hpp"

#include <stdio.h>

#include <algorithm>
#include <iostream>
#include <boost/date_time/posix_time/posix_time.hpp>

namespace terark { namespace wordseg {

//typedef XIdentity DictGetCharCode;
typedef char_tolower DictGetCharCode;

DictForward::DictForward(std::wstring& pool, const std::vector<std::pair<int,int> >& voffval, bool hasDupWord, bool isSorted)
{
	using namespace std;
	assert(voffval.size() >= 1);
	assert(pool.size() == (size_t)voffval.back().first);

	if (hasDupWord || !isSorted)
	{
		vector<TokenWord> tokens(voffval.size() - 1);
		const wchar_t* first = pool.data();
		for (size_t i = 0; i != tokens.size(); ++i)
		{
			pair<int,int> x = voffval[i];
			tokens[i] = TokenWord(const_wstring(first + x.first, first + voffval[i+1].first), x.second);
		}
		CompareWordsW comp;
		if (isSorted) {
#if defined(_DEBUG) || !defined(NDEBUG)
			for (vector<TokenWord>::iterator i = tokens.begin(); i < tokens.end()-1; ++i)
			{
#if !defined(_MSC_VER)
				std::string w0 = wcs2str(i[0]);
				std::string w1 = wcs2str(i[1]);
#endif
				int iret = comp.compare(i[0], i[1]);
				assert(iret <= 0);
			}
#endif
		} else {
			std::sort(tokens.begin(), tokens.end(), comp);
		//	std::sort(&*tokens.begin(), &*tokens.begin() + tokens.size(), comp);
		}
		if (hasDupWord)
		{
			vector<TokenWord>::iterator iter = set_unique(tokens.begin(), tokens.end(), not2(comp));
			if (iter != tokens.end())
				tokens.erase(iter, tokens.end());
		} else {
#if defined(_DEBUG) || !defined(NDEBUG)
			for (vector<TokenWord>::iterator i = tokens.begin(); i < tokens.end()-1; ++i)
			{
#if !defined(_MSC_VER)
				std::string w0 = wcs2str(i[0]);
				std::string w1 = wcs2str(i[1]);
#endif
				int iret = comp.compare(i[0], i[1]);
				assert(iret < 0);
			}
#endif
		}
		m_val.resize(tokens.size() + 1); // use m_val as offset to m_pool
		m_pool.reserve(pool.size());
		for (vector<TokenWord>::const_iterator i = tokens.begin(); i != tokens.end(); ++i)
		{
			m_val[i - tokens.begin()] = m_pool.size(); // use m_val as offset to m_pool
			m_pool.append(i->begin(), i->end());
		}
		m_val.back() = m_pool.size();
		wstring().swap(pool); // clean pool

		m_index.resize(tokens.size() + 1);
		for (size_t i = 0; i != tokens.size(); ++i)
		{
			int offset = m_val[i];
			m_index[i] = m_pool.data() + offset;
			m_val[i] = tokens[i].freq; // put val to m_val
		}
		m_index.back() = m_pool.data() + m_pool.size();

		// 现在 m_val 比实际需要的尺寸大 1, 把它设为需要的尺寸
		m_val.resize(m_val.size() - 1);
	}
	else // more fast
	{
		m_pool.swap(pool); // get pool
		m_index.resize(voffval.size());
		m_val.resize(voffval.size() - 1);
		for (size_t i = 0; i != m_val.size(); ++i)
		{
			pair<int,int> iv = voffval[i];
			m_index[i] = m_pool.data() + iv.first;
			m_val[i] = iv.second;
		}
		m_index.back() = m_pool.data() + m_pool.size();
	}
	m_minChar = DictGetCharCode()(m_index[0][0]);
	m_maxChar = DictGetCharCode()(m_index.end()[-2][0]);
	int curChar = m_minChar;
	m_iidx.resize(m_maxChar - m_minChar + 2);
	index_t::const_iterator j = m_index.begin(), jend = m_index.end()-1;
	for (; j != jend;)
	{
		StrCharPos_Searcher<wchar_t, GetCharAtPos, DictGetCharCode> comp;
		index_t::const_iterator next = comp.upper_bound(j, jend, *j, 0);

		m_iidx[curChar-m_minChar] = j - m_index.begin();
		++curChar;
		if (jend != next)
		{
			int nextChar = (*next)[0];
			for (; curChar < nextChar; ++curChar)
				m_iidx[curChar-m_minChar] = next - m_index.begin();
		}
		j = next;
	}
	assert(curChar == m_maxChar + 1);
	m_iidx[m_maxChar-m_minChar+1] = m_index.size()-1;
}

int DictForward::lower_bound(index_t::const_iterator _First, index_t::const_iterator _Last, const_wstring word)
{
	int _Count = _Last - _First;
	for (; 0 < _Count; )
		{	// divide and conquer, find half that contains answer
		int _Count2 = _Count / 2;
		index_t::const_iterator _Mid = _First + _Count2;
		const_wstring _Wmid(*_Mid, *(_Mid+1));
		if (CompareWordsW()(_Wmid, word))
			_First = ++_Mid, _Count -= _Count2 + 1;
		else
			_Count = _Count2;
		}
	return (_First - m_index.begin());
}

int DictForward::lower_bound(const_wstring word)
{
	const wchar_t ch = DictGetCharCode()(word[0]);
	if (ch < m_minChar)
	{
		return 0;
	}
	if (ch > m_maxChar)
	{
		return m_val.size();
	}
	typedef std::pair<index_t::const_iterator,index_t::const_iterator> ii_t;
	ii_t jj(m_index.begin() + m_iidx[ch - m_minChar],
			m_index.begin() + m_iidx[ch - m_minChar + 1]);
	if (jj.second == jj.first)
	{
		return jj.first - m_index.begin();
	}
	return lower_bound(m_index.begin(), m_index.end()-1, word);
}

int DictForward::upper_bound(index_t::const_iterator _First, index_t::const_iterator _Last, const_wstring word)
{
	int _Count = _Last - _First;
	for (; 0 < _Count; )
		{	// divide and conquer, find half that contains answer
		int _Count2 = _Count / 2;
		index_t::const_iterator _Mid = _First + _Count2;
		const_wstring _Wmid(*_Mid, *(_Mid+1));
		if (!CompareWordsW()(word, _Wmid))
			_First = ++_Mid, _Count -= _Count2 + 1;
		else
			_Count = _Count2;
		}
	return (_First - m_index.begin());
}

int DictForward::upper_bound(const_wstring word)
{
	const wchar_t ch = DictGetCharCode()(word[0]);
	if (ch < m_minChar)
	{
		return 0;
	}
	if (ch > m_maxChar)
	{
		return m_val.size();
	}
	typedef std::pair<index_t::const_iterator,index_t::const_iterator> ii_t;
	ii_t jj(m_index.begin() + m_iidx[ch - m_minChar],
			m_index.begin() + m_iidx[ch - m_minChar + 1]);
	if (jj.second == jj.first)
	{
		return jj.first - m_index.begin();
	}
	return upper_bound(m_index.begin(), m_index.end()-1, word);
}

std::pair<int,int> DictForward::equal_range(index_t::const_iterator _First, index_t::const_iterator _Last, const_wstring word)
{
	int _Count = _Last - _First;
	for (; 0 < _Count; )
	{	// divide and conquer, check midpoint
		int _Count2 = _Count / 2;
		index_t::const_iterator _Mid = _First + _Count2;
		const_wstring _Wmid(*_Mid, *(_Mid+1));

		int cmp = CompareWordsW().compare(_Wmid, word);
		if (cmp < 0)
		{	// range begins above _Mid, loop
			_First = ++_Mid;
			_Count -= _Count2 + 1;
		}
		else if (cmp > 0)
			_Count = _Count2;	// range in first half, loop
		else
		{	// range straddles mid, find each end and return
			int _First2 = lower_bound(_First, _Mid, word);
			int _Last2 = upper_bound(++_Mid, _First + _Count, word);
			return (std::pair<int,int>(_First2, _Last2));
		}
	}
	int idx = _First - m_index.begin();
	return std::pair<int,int>(idx,idx);	// empty range
}

std::pair<int,int> DictForward::equal_range(const_wstring word)
{
	const wchar_t ch = DictGetCharCode()(word[0]);
	if (ch < m_minChar)
	{
		return std::pair<int,int>(0,0);
	}
	if (ch > m_maxChar)
	{
		return std::pair<int,int>(m_val.size(), m_val.size());
	}
	typedef std::pair<index_t::const_iterator,index_t::const_iterator> ii_t;
	ii_t jj(m_index.begin() + m_iidx[ch - m_minChar],
			m_index.begin() + m_iidx[ch - m_minChar + 1]);
	if (jj.second == jj.first)
	{
		return std::pair<int,int>(jj.first - m_index.begin(), jj.second - m_index.begin());
	}
	return equal_range(jj.first, jj.second, word);
}

DictBackward::DictBackward(const DictForward& dforward)
	: m_index(dforward.index().size()-1)
	, m_fore_idx(dforward.index().size()-1)
	, m_len(dforward.index().size()-1)
	, m_fore(&dforward)
{
	using namespace std;

	std::vector<TokenWord> cwvec(dforward.index().size()-1);
	for (size_t i = 0; i != cwvec.size(); ++i)
	{
		cwvec[i] = TokenWord(dforward.str(i), -1, i);
	}
	std::sort(cwvec.begin(), cwvec.end(), CompareWordsBackward());

	for (size_t i = 0; i != cwvec.size(); ++i)
	{
		const TokenWord& x = cwvec[i];
		m_len[i] = x.size();
		m_index[i] = x.end();
		m_fore_idx[i] = x.widx;
	}
	m_minChar = DictGetCharCode()(m_index[0][-1]);
	m_maxChar = DictGetCharCode()(m_index.end()[-1][-1]);
	int curChar = m_minChar;
	m_iidx.resize(m_maxChar - m_minChar + 2);
	index_t::const_iterator j = m_index.begin(), jend = m_index.end();
	for (; j != jend;)
	{
		StrCharPos_Searcher<wchar_t, GetCharAtPos, DictGetCharCode> comp;
		index_t::const_iterator next = comp.upper_bound(j, jend, *j, -1);

		m_iidx[curChar-m_minChar] = j - m_index.begin();
		++curChar;
		if (m_index.end() != next)
		{
			int nextChar = (*next)[-1];
			for (; curChar < nextChar; ++curChar)
				m_iidx[curChar-m_minChar] = next - m_index.begin();
		}
		j = next;
	}
	assert(curChar == m_maxChar + 1);
	m_iidx[m_maxChar-m_minChar+1] = m_index.size();
}

std::vector<TokenWord>
DictWordSeg_Forward::seg(const wchar_t* first, const wchar_t* last) const
{
	using namespace std;
	vector<TokenWord> words;
	assert(!m_val.empty());
	if (m_val.empty())
		return words;
	std::vector<int> freq0(this->size(), 0);
	typedef std::pair<index_t::const_iterator,index_t::const_iterator> ii_t;
	vector<index_t::const_iterator> backTrace; backTrace.reserve(100);
	int line = 0;
	const wchar_t* lineBeg = first;
	for (const wchar_t* iter = first; iter < last;)
	{
		if (*iter == '\n')
		{
			++iter;
			++line;
			lineBeg = iter;
			++m_cBranch[0];
			continue;
		}
		const wchar_t ch = DictGetCharCode()(*iter);
		if (ch < m_minChar || ch > m_maxChar)
		{
			++iter;
			++m_cBranch[1];
			continue;
		}
		ii_t jj(m_index.begin() + m_iidx[ch - m_minChar],
				m_index.begin() + m_iidx[ch - m_minChar + 1]);
		if (jj.second == jj.first)
		{
			++iter;
			++m_cBranch[2];
			continue;
		}
		assert(backTrace.size() == 0);
		StrCharPos_Searcher<wchar_t, GetCharAtPos, DictGetCharCode> comp;
		const wchar_t* nextWord = iter + 1;
		int pos = 0;
		while (jj.second != jj.first)
		{
		#if defined(_DEBUG) || !defined(NDEBUG)
			int watch_range_size = jj.second - jj.first; (void)watch_range_size;
		#endif
			++pos;
			backTrace.push_back(jj.first);
			if (iter + pos >= last)
				break;
			if (pos >= this->len(jj.first))
			{
				if (++jj.first == jj.second)
				{
				//	cout << "goto, word=\"" << wcs2str(iter, iter+pos) << "\", ";
					break;
				}
			}
			assert(pos < this->len(jj.first));
			jj = comp.equal_range(jj.first, jj.second, iter, pos);
		}

		if (1 == pos) // 只匹配了第一个字
			++m_cBranch[3];
		else
			++m_cBranch[4];

		while (pos > 0)
		{
			index_t::const_iterator iWord = backTrace[pos-1];

			const int size = this->len(iWord);
			const wchar_t* wordend = iter + size;

		//	cout << "\"" << wcs2str(iter, iter+pos) << "\", ";
			if (size == pos)
			{
				// 找到了这个词
				// word 边界不能同时为字母数字，也就说：
				// 当前 word 的第一个字符和紧邻当前 word 的前一个字符不能同时为字母数字
				// 当前 word 的最后一个字符和紧邻当前 word 的下一个字符不能同时为字母数字
				if ((wordend == last || !(ws_isw_asc_alnum(wordend[-1]) && ws_isw_asc_alnum(*wordend))) &&
					(iter == first || !(ws_isw_asc_alnum(iter[-1]) && ws_isw_asc_alnum(*iter))))
				{
					const_wstring word(iter, wordend);
					TokenWord tw(word, 0);
					tw.widx = this->nth(iWord);
					tw.ntho = freq0[tw.widx]++;
					tw.line = line;
					tw.colu = iter - lineBeg;
					words.push_back(tw);
					nextWord = wordend;
					break;
				}
			}
			pos--;
		}
		backTrace.resize(0);
		iter = max(nextWord, iter+1);
	}
	for (size_t i = 0; i != words.size(); ++i)
	{
		words[i].freq = freq0[words[i].widx];
	}
	return words;
}

std::vector<TokenWord>
DictWordSeg_Backward::seg(const wchar_t* first, const wchar_t* last) const
{
	using namespace std;

	typedef std::pair<index_t::const_iterator,index_t::const_iterator> ii_t;

	vector<TokenWord> words;
//	assert(!m_fore_idx.empty());
	if (m_fore_idx.empty())
		return words;

	std::vector<int> freq0(this->size(), 0);
	vector<index_t::const_iterator> backTrace; backTrace.reserve(100);
	int line = 0;
	const wchar_t* lineEnd = last;
	for (const wchar_t* iter = last; iter > first;)
	{
		if (iter[-1] == L'\n')
		{
			if (!words.empty())
			{
				int max_colu = lineEnd - iter;
				vector<TokenWord>::iterator j = words.end();
				for (;;) {
					--j;
					if (j->line == line)
						j->colu = max_colu - j->colu;
					else
						break;
					if (words.begin() == j)
						break;
				}
			}
			lineEnd = iter;
			++line;
			--iter;
			++m_cBranch[0];
			continue;
		}
		const wchar_t ch = DictGetCharCode()(iter[-1]);
		if (ch < m_minChar || ch > m_maxChar)
		{
			--iter;
			++m_cBranch[1];
			continue;
		}
		ii_t jj(m_index.begin() + m_iidx[ch - m_minChar],
				m_index.begin() + m_iidx[ch - m_minChar+1]);
		if (jj.second == jj.first)
		{
			--iter;
			++m_cBranch[2];
			continue;
		}

		assert(backTrace.size() == 0);
		StrCharPos_Searcher<wchar_t, GetCharAtPos, DictGetCharCode> comp;
		const wchar_t* nextWord = iter - 1;
		int pos = -1;
		while (jj.second != jj.first)
		{
		#if defined(_DEBUG) || !defined(NDEBUG)
			int watch_range_size = jj.second - jj.first; (void)watch_range_size;
		#endif
			--pos;
			backTrace.push_back(jj.first);
			if (iter + pos < first)
				break;
			if (-pos > this->len(jj.first))
			{
				if (++jj.first == jj.second)
					break;
			}
			assert(-pos <= this->len(jj.first));
			jj = comp.equal_range(jj.first, jj.second, iter, pos);
		}

		if (-2 == pos) // 只匹配了第一个字
			++m_cBranch[3];
		else
			++m_cBranch[4];

		while (pos < -1)
		{
			index_t::const_iterator iWord = backTrace[-pos-1-1];
			const int neg_size = -this->len(iWord);
			const wchar_t* chbeg = iter + neg_size;
		//	cout << "\"" << wcs2str(iter+pos, iter) << "\", ";
			if (neg_size == pos + 1)
			{
			// 找到了这个词
			// word 边界不能同时为字母数字，也就说：
			// 当前 word 的第一个字符和紧邻当前 word 的前一个字符不能同时为字母数字
			// 当前 word 的最后一个字符和紧邻当前 word 的下一个字符不能同时为字母数字
				if ((chbeg == first || !(ws_isw_asc_alnum(chbeg[-1]) && ws_isw_asc_alnum(*chbeg))) &&
					(iter == last || !(ws_isw_asc_alnum(iter[-1]) && ws_isw_asc_alnum(*iter))))
				{
					const_wstring word(iter + neg_size, iter);
					TokenWord tw(word, 0);
					tw.widx = this->fnth(iWord);
					tw.ntho = freq0[tw.widx]++;
					tw.line = line;
					tw.colu = lineEnd - word.begin();
					words.push_back(tw);
					nextWord = chbeg;
					break;
				}
			}
			pos++;
		}
		backTrace.resize(0);
		iter = min(nextWord, iter-1);
	}
	if (!words.empty() && L'\n' != *first)
	{
		int max_colu = lineEnd - first - 1;
		vector<TokenWord>::iterator j = words.end();
		for (;;) {
			--j;
			if (j->line == line)
				j->colu = max_colu - j->colu;
			else
				break;
			if (words.begin() == j)
				break;
		}
	}
	for (std::vector<TokenWord>::iterator i = words.begin(); i != words.end(); ++i)
	{
		i->freq = freq0[i->widx];
		i->ntho = i->freq - i->ntho - 1;
		i->line = line - i->line;
	}
	std::reverse(words.begin(), words.end());
	return words;
}

std::vector<TokenWord>
DictWordSeg_Double::seg(const wchar_t* first, const wchar_t* last) const
{
	using namespace std;
	assert(m_fore->size() > 0);
	if (m_fore->size() == 0)
		return std::vector<TokenWord>();

	vector<TokenWord> wordsFore = m_fore->seg(first, last);
	vector<TokenWord> wordsBack = m_back->seg(first, last);
	return fb_union(wordsFore, wordsBack);
}

std::vector<TokenWord>
DictWordSeg_Double::fb_union(
	const std::vector<TokenWord>& wordsFore,
	const std::vector<TokenWord>& wordsBack
  ) const
{
	using namespace std;

	vector<TokenWord> wordsDouble;
	wordsDouble.reserve(max(wordsFore.size(), wordsBack.size())*3/2);
	vector<TokenWord>::const_iterator i = wordsFore.begin(), j = wordsBack.begin();
	while (wordsFore.end() != i && wordsBack.end() != j)
	{
		bool bOverlap = false;
		if (i->begin() == j->begin())
		{
			bOverlap = true;
		}
		else if (i->begin() < j->begin())
		{
			if (j->begin() < i->end())
				bOverlap = true; // j->begin in (i->begin, i->end)
			else {// i->end <= j->begin
				if (wordsDouble.empty() || wordsDouble.back().end() < i->end())
				{
					wordsDouble.push_back(*i);
				}
				++i;
			}
		}
		else //if (j->begin() < i->begin())
		{
			if (i->begin() < j->end())
				bOverlap = true; // i->begin in (j->begin, j->end)
			else {// j->end <= i->begin
				if (wordsDouble.empty() || wordsDouble.back().end() < j->end())
				{
					wordsDouble.push_back(*j);
				}
				++j;
			}
		}

		if (bOverlap)
		{
			if (i->size() < j->size())
			{
			//	cerr<< "conflict[b]: forward\"" << wcs2str(f)
			//		<< "\", backward\"" << wcs2str(b) << "\"" << endl;
				wordsDouble.push_back(*j);
			}
			else if (j->size() < i->size())
			{
			//	cerr<< "conflict[f]: forward\"" << wcs2str(f)
			//		<< "\", backward\"" << wcs2str(b) << "\"" << endl;
				wordsDouble.push_back(*i);
			}
			else if (i->begin() < j->begin())
			{
			//	cerr<< "conflict[bb]: forward\"" << wcs2str(f)
			//		<< "\", backward\"" << wcs2str(b) << "\"" << endl;
				wordsDouble.push_back(*j);
			}
			else if (j->begin() < i->begin())
			{
			//	cerr<< "conflict[ff]: forward\"" << wcs2str(f)
			//		<< "\", backward\"" << wcs2str(b) << "\"" << endl;
				wordsDouble.push_back(*i);
			}
			else
			{
				wordsDouble.push_back(*i);
			}
			++i; ++j;
		}
	}
	for (; wordsFore.end() != i; ++i)
	{
		wordsDouble.push_back(*i);
	}
	for (; wordsBack.end() != j; ++j)
	{
		wordsDouble.push_back(*j);
	}
	return wordsDouble;
}

std::vector<TokenWord>
DictWordSeg_Double::simple_union(
	const std::vector<TokenWord>& wordsFore,
	const std::vector<TokenWord>& wordsBack
  ) const
{
	using namespace std;

	vector<TokenWord> wordsDouble;
	wordsDouble.reserve(max(wordsFore.size(), wordsBack.size())*3/2);
	vector<TokenWord>::const_iterator i = wordsFore.begin(), j = wordsBack.begin();
	while (wordsFore.end() != i && wordsBack.end() != j)
	{
		if (i->begin() < j->begin())
		{
			wordsDouble.push_back(*i);
			++i;
		}
		else if (j->begin() < i->begin())
		{
			wordsDouble.push_back(*j);
			++j;
		}
		else if (i->end() < j->end())
		{
			wordsDouble.push_back(*i);
			++i;
		}
		else if (j->end() < i->end())
		{
			wordsDouble.push_back(*j);
			++j;
		}
		else
		{
			wordsDouble.push_back(*i);
			++i; ++j;
		}
	}
	for (; wordsFore.end() != i; ++i)
	{
		wordsDouble.push_back(*i);
	}
	for (; wordsBack.end() != j; ++j)
	{
		wordsDouble.push_back(*j);
	}
	return wordsDouble;
}

DictWordSeg_Forward*
load_dict(const char* dictname, const char* sortedname, std::ostream& out, std::ostream& err)
{
	using namespace std;
	using namespace boost::posix_time;

	ptime t0 = microsec_clock::local_time();

	FILE* fdict = NULL;
	FILE* fsorted = fopen(sortedname, "r");
	if (NULL == fsorted)
	{
		fdict = fopen(dictname, "r");
		if (NULL == fdict)
		{
			err << "can not open dict file: '" << dictname << "'" << std::endl;
			return NULL;
		}
	} else
		fdict = fsorted;

	wstring strPool;
	out << "read '" << (0 == fsorted ? dictname : sortedname) << "' ..." << std::endl;
	vector<pair<int,int> > voffval;
	while (!feof(fdict))
	{
		char word[104];
		int val;
		int nField = fscanf(fdict, "%s%d%*[^\n]\n", word, &val);
		if (2 == nField)
		{
			dict_add_word(strPool, voffval, str2wcs(const_string(word)), val);
		}
	}
	voffval.push_back(make_pair(int(strPool.size()), 0));

	ptime t1 = microsec_clock::local_time();
	bool hasDupWord = true;
	bool isSorted = 0 != fsorted;
	DictWordSeg_Forward* foreward = new DictWordSeg_Forward(strPool, voffval, hasDupWord, isSorted);
	ptime t2 = microsec_clock::local_time();

	if (0 == fsorted)
	{
		fsorted = fopen(sortedname, "w+");
		if (0 == fsorted) {
			err << "can not open \"" << sortedname << "\" for write!\n";
			fclose(fdict);
			return foreward;
		}
		for (int i = 0; i != foreward->size(); ++i)
		{
			fprintf(fsorted, "%s\t%d\n",
				wcs2str(foreward->str(i)).c_str(),
				foreward->val(i));
		}
		fclose(fsorted);
	}
	ptime t3 = microsec_clock::local_time();
	fclose(fdict);

	out << "read '" << (fsorted == fdict ? sortedname : dictname) << "' completed, "
		<< voffval.size() << " words"
		<< " time(millisec)[parse=" << (t1-t0).total_milliseconds()
		<< ", sort="  << (t2-t1).total_milliseconds()
		<< ", write=" << (t3-t2).total_milliseconds()
		<< "]" << std::endl;

	return foreward;
}

} } // namespace terark::wordseg

#include "TokenWord.hpp"
#include "code_convert.hpp"
#include <iomanip>

namespace terark { namespace wordseg {

std::ostream& operator<<(std::ostream& os, const TokenWord_CA& x)
{
	using namespace std;
	os	<< "(word=" << string(x.begin(), x.end())
		<< ", freq=" << setw(4) << x.freq
		<< ", widx=" << setw(8) << x.widx
		<< ")"
		;
	return os;
}

std::ostream& operator<<(std::ostream& os, const TokenWord_CW& x)
{
	using namespace std;
	os	<< "(word=" << wcs2str(x)
		<< ", freq=" << setw(4) << x.freq
		<< ", widx=" << setw(8) << x.widx
		<< ")"
		;
	return os;
}

std::ostream& operator<<(std::ostream& os, const TokenWord_SA& x)
{
	using namespace std;
	os	<< "(word=" << x.word()
		<< ", freq=" << setw(4) << x.freq
		<< ", widx=" << setw(8) << x.widx
		<< ")"
		;
	return os;
}

std::ostream& operator<<(std::ostream& os, const TokenWord_SW& x)
{
	using namespace std;
	os	<< "(word=" << wcs2str(x)
		<< ", freq=" << setw(4) << x.freq
		<< ", widx=" << setw(8) << x.widx
		<< ")"
		;
	return os;
}

void printTokenWord(std::ostream& ost, const TokenWord& tw, int wordwidth, const wchar_t* base)
{
	using namespace std;

	ost << "(";
	ost << left << setw(wordwidth) << wcs2str(tw.word());
	if (0 != base)
	{
		ost << ' ' << right << setw(6) << (tw.begin() - base);
		ost << ' ' << right << setw(6) << (tw.end()   - base);
	}
	ost << ' ' << right << setw(5) << tw.line;
	ost << ' ' << right << setw(5) << tw.colu;
	ost << ' ' << right << setw(4) << tw.freq;
	ost << ' ' << right << setw(4) << tw.ntho;
	ost << ' ' << right << setw(8) << tw.widx;
	ost << ")\n";
}

void printTokenWords(std::ostream& ost, const std::vector<TokenWord>& tw, const wchar_t* base)
{
	using namespace std;

	size_t wordwidth = 0;
	for (std::vector<TokenWord>::const_iterator i = tw.begin(); i != tw.end(); ++i)
	{
		size_t w2 = 0;
		for (TokenWord::const_iterator cp = i->begin(); cp != i->end(); ++cp)
			w2 += 1 + (*cp > 127);
		wordwidth = max(wordwidth, w2);
	}
	ost << "(" << left << setw(wordwidth+1) << "word";
	if (0 != base)
		ost << "    beg    end ";
	ost << " line  colu freq ntho     widx), all 0 based" << endl;
	for (std::vector<TokenWord>::const_iterator i = tw.begin(); i != tw.end(); ++i)
	{
		printTokenWord(ost, *i, wordwidth+1, base);
	}
	ost << endl;
}

void printTokenWord(std::ostream& ost, const TokenWord_SA& tw, int wordwidth)
{
	using namespace std;

	ost << "(";
	ost << left  << setw(wordwidth) << tw.word();
	ost << ' ' << right << setw(5) << tw.line;
	ost << ' ' << right << setw(5) << tw.colu;
	ost << ' ' << right << setw(4) << tw.freq;
	ost << ' ' << right << setw(4) << tw.ntho;
	ost << ' ' << right << setw(8) << tw.widx;
	ost << ")\n";
}

void printTokenWords(std::ostream& ost, const std::vector<TokenWord_SA>& tw)
{
	using namespace std;

// 	bool  bPrint_line = false
// 		, bPrint_colu = false
// 		, bPrint_ntho = false
// 		, bPrint_widx = false
// 		;
	size_t wordwidth = 0;
	for (int i = 0; i != tw.size(); ++i)
	{
		wordwidth = max(wordwidth, tw[i].size());
	}
	ost << "(" << left << setw(wordwidth) << "word";
// 	if (bPrint_line)
// 		ost << " line  ";
// 	if (bPrint_colu)
// 		ost << "colu ";
// 	if (bPrint_ntho)
// 		ost << " line ";
// 	if (bPrint_widx)
// 		ost << "     widx ";
	ost << " line  colu freq ntho     widx), all 0 based" << endl;
	for (int i = 0; i != tw.size(); ++i)
	{
		printTokenWord(ost, tw[i], wordwidth);
	}
	ost << endl;
}



} } // namespace terark::wordseg

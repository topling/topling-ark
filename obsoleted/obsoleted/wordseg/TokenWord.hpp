#ifndef __terark_wordseg_TokenWord_h__
#define __terark_wordseg_TokenWord_h__

#include <terark/config.hpp>
#include "../const_string.hpp"
#include <iostream>
#include <vector>

namespace terark { namespace wordseg {

template<class CharT>
struct TokenWord_CT : public basic_const_string<CharT>
{
	typedef  std::basic_string<CharT> Mysstr_t;
	typedef basic_const_string<CharT> Mycstr_t;

	int freq;
	int widx;
	int ntho; //!< nth occurence
	int line;
	int colu;
	uint32_t idw; // permanent ID_Word, 1 based, 0 indicator no ID

	TokenWord_CT() {}

	TokenWord_CT(Mycstr_t word, int freq, int widx = -1, int ntho = -1)
		: Mycstr_t(word)
		, freq(freq)
		, widx(widx)
		, ntho(ntho)
		, line(-1)
		, colu(-1)
		, idw(0)
	{}
	TokenWord_CT(const CharT* first, const CharT* last, int freq, int widx = -1, int ntho = -1)
		: Mycstr_t(first, last)
		, freq(freq)
		, widx(widx)
		, ntho(ntho)
		, line(-1)
		, colu(-1)
		, idw(0)
	{}

	int area() const { return this->size() * freq; }

	const Mycstr_t& word() const { return *this; }
	Mycstr_t& word() { return *this; }
};
typedef TokenWord_CT< char  > TokenWord_CA;
typedef TokenWord_CT<wchar_t> TokenWord_CW, TokenWord;

template<class CharT>
struct TokenWord_ST : public std::basic_string<CharT>
{
	typedef  std::basic_string<CharT> Mysstr_t;
	typedef basic_const_string<CharT> Mycstr_t;

	int freq;
	int widx; //!<
	int ntho; //!< nth occurence
	int line;
	int colu;
	uint32_t idw; // permanent ID_Word, 1 based, 0 indicator no ID

	TokenWord_ST() {}

	TokenWord_ST(const Mycstr_t& word, int freq, int widx = -1, int ntho = -1)
		: Mysstr_t(word.begin(), word.end())
		, freq(freq)
		, widx(widx)
		, ntho(ntho)
		, line(-1)
		, colu(-1)
		, idw(0)
	{}

	TokenWord_ST(const Mysstr_t& word, int freq, int widx = -1, int ntho = -1)
		: Mysstr_t(word)
		, freq(freq)
		, widx(widx)
		, ntho(ntho)
		, line(-1)
		, colu(-1)
		, idw(0)
	{}

	TokenWord_ST(const TokenWord_CT<CharT>& y)
		: Mysstr_t(y.begin(), y.end())
		, freq(y.freq)
		, widx(y.widx)
		, ntho(y.ntho)
		, line(y.line)
		, colu(y.colu)
		, idw(y.idw)
	{
	}

	template<class OtherCharT>
	TokenWord_ST(const TokenWord_CT<OtherCharT>& y)
		: Mysstr_t(ss_cvt(y.word()))
		, freq(y.freq)
		, widx(y.widx)
		, ntho(y.ntho)
		, line(y.line)
		, colu(y.colu)
		, idw(y.idw)
	{
	}

	template<class OtherCharT>
	TokenWord_ST(const TokenWord_ST<OtherCharT>& y)
		: Mysstr_t(ss_cvt(y.word()))
		, freq(y.freq)
		, widx(y.widx)
		, ntho(y.ntho)
		, line(y.line)
		, colu(y.colu)
		, idw(y.idw)
	{
	}

	int area() const { return this->size() * freq; }

	const Mysstr_t& word() const { return *this; }
	Mysstr_t& word() { return *this; }
};
typedef TokenWord_ST< char  > TokenWord_SA;
typedef TokenWord_ST<wchar_t> TokenWord_SW;

TERARK_DLL_EXPORT std::ostream& operator<<(std::ostream& os, const TokenWord_CA& x);
TERARK_DLL_EXPORT std::ostream& operator<<(std::ostream& os, const TokenWord_CW& x);
TERARK_DLL_EXPORT std::ostream& operator<<(std::ostream& os, const TokenWord_SA& x);
TERARK_DLL_EXPORT std::ostream& operator<<(std::ostream& os, const TokenWord_SW& x);

TERARK_DLL_EXPORT void printTokenWord(std::ostream& ost, const TokenWord& tw, int wordwidth, const wchar_t* base = 0);
TERARK_DLL_EXPORT void printTokenWords(std::ostream& ost, const std::vector<TokenWord>& tw, const wchar_t* base = 0);

TERARK_DLL_EXPORT void printTokenWord(std::ostream& ost, const TokenWord_SA& tw, int wordwidth);
TERARK_DLL_EXPORT void printTokenWords(std::ostream& ost, const std::vector<TokenWord_SA>& tw);

} } // namespace terark::wordseg

#endif


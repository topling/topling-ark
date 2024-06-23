/* vim: set tabstop=4 : */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <iostream>

#if defined(_MSC_VER)
#  if defined(_WIN32) || defined(_WIN64)
#    define WIN32_LEAN_AND_MEAN
#    define NOMINMAX
#    include <windows.h>
#  endif
#else
#  include <iconv.h>
#  include <boost/thread.hpp>
#endif

#include "code_convert.hpp"

namespace terark { namespace wordseg {

#ifndef _MSC_VER
iconv_t G_gb_to_wchar;
iconv_t G_wchar_to_gb;
boost::mutex G_iconv_mutex;

void init_iconv()
{
	static bool inited = false;
	if (!inited)
	{
		char szUTF[32];
		sprintf(szUTF, "UTF%d", (int)sizeof(wchar_t)*8);
		G_gb_to_wchar = iconv_open(szUTF, "GB18030");
		G_wchar_to_gb = iconv_open("GB18030", szUTF);
	//	iconv(G_gb_to_wchar, NULL, NULL, NULL, NULL);
	//	iconv(G_wchar_to_gb, NULL, NULL, NULL, NULL);
	}
	inited = true;
}
#endif

std::string wcs2str(const wchar_t* first, const wchar_t* last)
{
	size_t len = (last - first);
#ifdef _MSC_VER
#if defined(_WIN32) || defined(_WIN64)
	std::string str; str.resize(sizeof(wchar_t)*(len));
	BOOL bUse;
	int n = WideCharToMultiByte(CP_ACP, 0, first, len, &*str.begin(), str.size(), NULL, &bUse);
	str.resize(n);
	return str;
#else
	std::string str; str.reserve(2*(len));
	for (const wchar_t* iter = first; iter != last; ++iter)
	{
		if (*iter & 0xFF80)
			str.append(1, char(wint_t(*iter))), str.append(1, char(wint_t(*iter) >> 8));
		else
			str.append(1, char(wint_t(*iter)));
	}
	return str;
#endif
#else
	boost::mutex::scoped_lock lock(G_iconv_mutex);
	init_iconv();
	size_t ngb = sizeof(wchar_t) * (len + 1);
	size_t nuc = sizeof(wchar_t) * (len + 0);
	std::string str; str.resize(ngb);
	char* gb = &*str.begin();
	char* uc = (char*)first;
	size_t n = iconv(G_wchar_to_gb, &uc, &nuc, &gb, &ngb);
	if (size_t(-1) == n) {
	// 	std::cout<< "wcs2str[nuc=(" << len*sizeof(wchar_t) << "," << nuc << "),"
	// 		<< "ngb=(" << str.size() << "," << ngb << "), nstr=" << (gb - &*str.begin()) << "]\n" << std::flush;
		perror(BOOST_CURRENT_FUNCTION);
		str.clear();
	} else {
		assert(gb - &*str.begin() >= ptrdiff_t(sizeof(char)*0));
		assert(gb - &*str.begin() <= ptrdiff_t(16*1024*1024));
		str.resize(gb - &*str.begin());
	}
	return str;
#endif
}
std::string wcs2str(const_wstring wcs)
{
	return wcs2str(wcs.begin(), wcs.end());
}

std::wstring str2wcs(const char* first, const char* last)
{
	size_t len = (last - first);
#ifdef _MSC_VER
#if defined(_WIN32) || defined(_WIN64)
	std::wstring wcs; wcs.resize(len);
	int n = MultiByteToWideChar(CP_ACP, 0, first, len, &*wcs.begin(), wcs.size());
	wcs.resize(n);
	return wcs;
#else
	std::wstring wcs; wcs.reserve(len);
	for (const char* iter = first; iter < last;)
	{
		if (*iter & 0x80)
			wcs.append(1, wchar_t(wint_t(*iter)|wint_t(iter[1]) << 8)), iter += 2;
		else
			wcs.append(1, wchar_t(*iter)), ++iter;
	}
	return wcs;
#endif
#else
	boost::mutex::scoped_lock lock(G_iconv_mutex);
	init_iconv();
	size_t ngb = len;
	size_t nuc = (len + 1)*sizeof(wchar_t);
	std::wstring wcs; wcs.resize(len + 1);
	char* uc = (char*)&*wcs.begin();
	char* gb = (char*)first;
	while (gb < last && nuc && ngb)
	{
		size_t n = iconv(G_gb_to_wchar, &gb, &ngb, &uc, &nuc);
		if (size_t(-1) == n)
		{
			if (EILSEQ == errno)
				++gb, --ngb; // ignore
			else if (EINVAL == errno)
				break;
			else
			{
			//	char szError[1024];
			//	strerror_r(errno, szError, sizeof(szError));
			//	fprintf(stderr, "iconv(G_gb_to_wchar) failed: %s\n", szError);
				fprintf(stderr, "iconv(G_gb_to_wchar) failed: %s\n", strerror(errno));
				assert(0);
				break;
			}
		}
	}
// 	std::cout<< "\"" << std::string(first, last) << "\", "
// 		<< "str2wcs[nuc=(" << len << "," << nuc << "),"
// 		<< "ngb=(" << len << "," << ngb << "), nwcs=" << ((wchar_t*)uc - &*wcs.begin()) << "]\n" << std::flush;
	assert(uc - (char*)&*wcs.begin() >= ptrdiff_t(sizeof(wchar_t))*0);
	assert(uc - (char*)&*wcs.begin() <= ptrdiff_t(sizeof(wchar_t))*16*1024*1024);

	if ((wchar_t*)uc > &*wcs.begin() && 0xFEFF == wcs[0])
		wcs.erase(wcs.begin()), uc -= sizeof(wchar_t);
	wcs.resize((wchar_t*)uc - &*wcs.begin());

	return wcs;
#endif
}

std::wstring str2wcs(const_string str)
{
	return str2wcs(str.begin(), str.end());
}

} }


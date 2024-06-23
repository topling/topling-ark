/* vim: set tabstop=4 : */
#ifndef __terark_wordseg_code_convert_h__
#define __terark_wordseg_code_convert_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#if defined(_WIN32) || defined(_WIN64)
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif
#endif

#include "../const_string.hpp"

namespace terark { namespace wordseg {

TERARK_DLL_EXPORT std::string wcs2str(const wchar_t* first, const wchar_t* last);
TERARK_DLL_EXPORT std::string wcs2str(const_wstring wcs);

TERARK_DLL_EXPORT std::wstring str2wcs(const char* first, const char* last);
TERARK_DLL_EXPORT std::wstring str2wcs(const_string str);

} }

#endif // __terark_wordseg_code_convert_h__

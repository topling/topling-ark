/* vim: set tabstop=4 : */
#include "compare.hpp"
#include "code_convert.hpp"

namespace terark { namespace wordseg {

std::wstring ss_cvt(const_string x)
{
	return str2wcs(x);
}

std::string ss_cvt(const_wstring x)
{
	return wcs2str(x);
}

int strncasecoll(const char* x, const char* y, size_t n1, size_t n2)
{
	using namespace std;
	std::wstring wx = str2wcs(x, x+n1);
	std::wstring wy = str2wcs(y, y+n2);
	return wcsncasecmp(wx.c_str(), wy.c_str(), min(wx.size(), wy.size()));
}

int strncoll(const char* x, const char* y, size_t n1, size_t n2)
{
	using namespace std;
	std::wstring wx = str2wcs(x, x+n1);
	std::wstring wy = str2wcs(y, y+n2);
	return wcsncmp(wx.c_str(), wy.c_str(), min(wx.size(), wy.size()));
}

} }


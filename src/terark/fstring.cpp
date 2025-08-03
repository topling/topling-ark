#include "fstring.hpp"
#include <ostream>
#include <ctype.h>

#if defined(_MSC_VER)
#include <boost/algorithm/searching/boyer_moore_horspool.hpp>
#endif

namespace terark {

template<class Char>
std::string basic_fstring<Char>::hex() const noexcept {
	constexpr ptrdiff_t nHexPerCH = 2 * sizeof(Char);
	std::string res;
	res.resize(nHexPerCH * n);
	char* buf = &res[0];
	static const char hexTab[] = "0123456789ABCDEF";
	for(ptrdiff_t i = 0; i < n; i++) {
		uc_t uc = p[i];
		for (ptrdiff_t j = 0; j < nHexPerCH; j++)
			buf[nHexPerCH*i + j] = hexTab[uc >> 4*(nHexPerCH-1-j) & 15];
	}
	return res;
}

//template std::string basic_fstring<char>::hex() const noexcept;
//template std::string basic_fstring<uint16_t>::hex() const noexcept;

std::string operator+(fstring x, fstring y) {
	std::string z;
	z.reserve(x.n + y.n);
	z.append(x.p, x.n);
	z.append(y.p, y.n);
	return z;
}

std::ostream& operator<<(std::ostream& os, fstring x) {
	os.write(x.data(), x.size());
	return os;
}

// fstring16

bool operator==(fstring16 x, fstring16 y) {
	if (x.n != y.n) return false;
	return memcmp(x.p, y.p, 2*x.n) == 0;
}
bool operator!=(fstring16 x, fstring16 y) { return !(x == y); }

bool operator<(fstring16 x, fstring16 y) {
	ptrdiff_t n = std::min(x.n, y.n);
	int ret = 0;
	for (ptrdiff_t i = 0; i < n; ++i) {
		uint16_t cx = x.p[i];
		uint16_t cy = y.p[i];
		if (cx != cy) {
			if (sizeof(int) > 2)
				ret = int(cx) - int(cy);
			else if (cx < cy)
				ret = -1;
			else
				ret = 1;
			break;
		}
	}
	if (ret)
		return ret < 0;
	else
		return x.n < y.n;
}
bool operator> (fstring16 x, fstring16 y) { return   y < x ; }
bool operator<=(fstring16 x, fstring16 y) { return !(y < x); }
bool operator>=(fstring16 x, fstring16 y) { return !(x < y); }

TERARK_DLL_EXPORT fstring var_symbol(const char* s) {
  const char* e = s;
  while (*e && ('_' == *e || isalnum((unsigned char)*e))) e++;
  return fstring(s, e);
}

#ifdef _MSC_VER
// boost 1.62- returns const Char*
// boost 1.63+ returns std::pair<const Char*, const Char*>
// make the brain dead boost happy
template<class Iter>
static inline
const Iter fuck_boost(std::pair<Iter,Iter> p) { return p.first; }
template<class Iter>
static inline
const Iter fuck_boost(Iter p) { return p; }

TERARK_DLL_EXPORT
char*
terark_fstrstr(const char* haystack, size_t haystack_len
			 , const char* needle, size_t needle_len)
{
	const char* hay_end = haystack + haystack_len;
	const char* needle_end = needle + needle_len;
	const char* q = fuck_boost(boost::algorithm::boyer_moore_horspool_search(
		haystack, hay_end, needle, needle_end));
	if (q == hay_end)
		return NULL;
	else
		return (char*)(q);
}
#endif

uint16_t*
terark_fstrstr(const uint16_t* haystack, size_t haystack_len
		   , const uint16_t* needle  , size_t needle_len)
{
#if defined(_MSC_VER)
	const uint16_t* hay_end = haystack + haystack_len;
	const uint16_t* needle_end = needle + needle_len;
	const uint16_t* q = fuck_boost(boost::algorithm::boyer_moore_horspool_search(
		haystack, hay_end, needle, needle_end));
	if (q == needle_end)
		return NULL;
	else
		return (uint16_t*)(q);
#else
	char*  hit = (char*)(haystack);
	char*  end = (char*)(haystack + haystack_len);
	ptrdiff_t needle_len2 = needle_len * 2;
	while (end - hit  >= needle_len2) {
		hit = (char*)memmem(hit, end - hit, needle, needle_len2);
		if (NULL == hit || (hit - (char*)haystack) % 2 == 0)
			return (uint16_t*)hit;
		hit++;
	}
#endif
	return NULL;
}

unsigned char gtab_ascii_tolower[256] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
	0x40, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,
	0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
	0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,
	0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F,
	0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F,
	0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F,
	0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF,
	0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF,
	0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF,
	0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF,
	0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
	0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF,
};

unsigned char gtab_ascii_toupper[256] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
	0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
	0x60, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F,
	0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F,
	0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F,
	0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF,
	0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF,
	0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF,
	0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF,
	0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
	0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF,
};

template<>
basic_fstring<char>& basic_fstring<char>::chomp() {
	while (n && isspace((unsigned char)(p[n-1]))) n--;
	return *this;
}
template<>
basic_fstring<uint16_t>& basic_fstring<uint16_t>::chomp() {
	while (n && iswspace(p[n-1])) n--;
	return *this;
}

template<>
basic_fstring<char>& basic_fstring<char>::trim() {
	while (n && isspace((unsigned char)(p[n-1]))) n--;
	while (n && isspace((unsigned char)(*p))) p++, n--;
	return *this;
}
template<>
basic_fstring<uint16_t>& basic_fstring<uint16_t>::trim() {
	while (n && iswspace(p[n-1])) n--;
	while (n && iswspace(*p)) p++, n--;
	return *this;
}

template struct basic_fstring<char>;
template struct basic_fstring<uint16_t>;

bool getEnvBool(const char* envName, bool Default) noexcept {
	if (const char* env = getenv(envName)) {
		if (isdigit(env[0])) {
			return atoi(env) != 0;
		}
#if defined(_MSC_VER)
		#define strcasecmp stricmp
#endif
		if (strcasecmp(env, "true") == 0)
			return true;
		if (strcasecmp(env, "false") == 0)
			return false;
		if (strcasecmp(env, "on" ) == 0) return true;
		if (strcasecmp(env, "off") == 0) return false;
		if (strcasecmp(env, "yes") == 0) return true;
		if (strcasecmp(env, "no" ) == 0) return false;
		fprintf(stderr
			, "WARN: terark::getEnvBool(\"%s\") = \"%s\" is invalid, treat as Default = %s\n"
			, envName, env, Default?"true":"false"
		);
	}
	return Default;
}

bool parseBooleanRelaxed(const char* str, bool Default) noexcept {
	if (isdigit(str[0])) {
		return atoi(str) != 0;
	}
	if (strcasecmp(str, "true") == 0)
		return true;
	if (strcasecmp(str, "false") == 0)
		return false;
	if (strcasecmp(str, "on" ) == 0) return true;
	if (strcasecmp(str, "off") == 0) return false;
	if (strcasecmp(str, "yes") == 0) return true;
	if (strcasecmp(str, "no" ) == 0) return false;
	fprintf(stderr
		, "WARN: terark::parseBooleanRelaxed(\"%s\") fail, use Default = %s\n"
		, str, Default?"true":"false"
	);
	return Default;
}

long getEnvLong(const char* envName, long Default) noexcept {
	if (const char* env = getenv(envName)) {
		int base = 0; // env can be oct, dec, hex
		return strtol(env, NULL, base);
	}
	return Default;
}

double getEnvDouble(const char* envName, double Default) noexcept {
  if (const char* env = getenv(envName)) {
    return strtof(env, NULL);
  }
  return Default;
}

const char* getEnvStr(const char* envName, const char* Default) {
  if (auto value = getenv(envName)) {
    return value;
  }
  else if (nullptr == Default) {
    // nullptr indicate envName must be defined
    THROW_STD(invalid_argument, "missing env var: %s", envName);
  }
  else
    return Default;
}

unsigned long long ParseSizeXiB(const char* str) noexcept {
	if (NULL == str || '\0' == *str) {
		return 0;
	}
    char* endp = NULL;
    double val = strtod(str, &endp);
    char scale = *endp;
    return ScaleSizeXiB(val, scale);
}
unsigned long long ParseSizeXiB(fstring str) noexcept {
    return ParseSizeXiB(str.c_str());
}
unsigned long long ParseSizeXiB(const char* str, const char* Default) noexcept {
	if (str) {
		char* endp = NULL;
		double val = strtod(str, &endp);
		if (endp != str)
			return ScaleSizeXiB(val, *endp);
	}
	return ParseSizeXiB(Default);
}
unsigned long long ParseSizeXiB(const char* str, unsigned long long Default) noexcept {
	if (str) {
		char* endp = NULL;
		double val = strtod(str, &endp);
		if (endp != str)
			return ScaleSizeXiB(val, *endp);
	}
	return Default;
}
unsigned long long ScaleSizeXiB(double val, char scale) noexcept {
    if ('k' == scale || 'K' == scale)
        return uint64_t(val * (1ull << 10));
    else if ('m' == scale || 'M' == scale)
        return uint64_t(val * (1ull << 20));
    else if ('g' == scale || 'G' == scale)
        return uint64_t(val * (1ull << 30));
    else if ('t' == scale || 'T' == scale)
        return uint64_t(val * (1ull << 40));
    else if ('p' == scale || 'P' == scale)
        return uint64_t(val * (1ull << 50));
    else
        return uint64_t(val);
}

// if quote == ("), escape (") as (\")
// if quote == ('), escape (') as (\')
// otherwise do not escape (') and (")
template<class PushBack>
void escape_append_imp(fstring str, char quote, PushBack push_back) {
	for(size_t  i = 0; i < size_t(str.n); ++i) {
		byte_t  c = str.p[i];
		switch (c) {
		default:
			if (c >= 0x20 && c <= 0x7E) { // isprint for ascii
				push_back(c);
			}
			else {
				const char* hex = "0123456789ABCDEF";
				push_back('\\');
				push_back('x');
				push_back(hex[c >> 4]);
				push_back(hex[c & 15]);
			}
			break;
		case '\\': // 0x27
			push_back('\\');
			push_back('\\');
			break;
		case '"': // 0x22
			if ('"' == quote)
				push_back('\\');
			push_back(c);
			break;
		case '\'': // 0x5C
			if ('\'' == quote)
				push_back('\\');
			push_back(c);
			break;
		case '\0':  push_back('\\'); push_back('0'); break; // 00
		case '\a':  push_back('\\'); push_back('a'); break; // 07
		case '\b':  push_back('\\'); push_back('b'); break; // 08
		case '\f':  push_back('\\'); push_back('f'); break; // 0C
		case '\n':  push_back('\\'); push_back('n'); break; // 0A
		case '\r':  push_back('\\'); push_back('r'); break; // 0D
		case '\t':  push_back('\\'); push_back('t'); break; // 09
		case '\v':  push_back('\\'); push_back('v'); break; // 0B
		}
	}
}

void escape_append(fstring str, std::string* res, char quote) {
	size_t esclen = 0;
	escape_append_imp(str, quote, [&esclen](byte_t) { esclen++; });
	size_t oldsize = res->size();
	res->resize(oldsize + esclen);
	char* p = &*res->begin() + oldsize;
	escape_append_imp(str, quote, [&p](byte_t c) { *p++ = char(c); });
	assert(&*res->begin() + res->size() == p);
}

std::string escape(fstring str, char quote) {
	std::string res;
	escape_append(str, &res, quote);
	return res;
}

#if defined(__GLIBCXX__)
void do_string_resize_no_touch_memory(std::string* bank, size_t sz);
template <typename Money_t, Money_t std::string::* p>
struct string_thief {
	friend
	void do_string_resize_no_touch_memory(std::string* bank, size_t sz) {
		// gcc bug: these lines of code can not be here:
		//   ERROR: use of local variable with automatic storage from
		//          containing function
		// if (terark_unlikely(sz > bank->capacity())) {
		// 	size_t old_size = bank->size();
		// 	size_t cap = std::max(sz, old_size * 103/64); // 103/64 <~ 1.618
		// 	bank->reserve(cap);
		// }
		(bank->*p)(sz);
	}
};
template struct string_thief<void(std::string::size_type),
							 &std::string::_M_length>;
void string_resize_no_touch_memory(std::string* bank, size_t sz) {
	if (terark_unlikely(sz > bank->capacity())) {
		size_t old_size = bank->size();
		size_t cap = std::max(sz, old_size * 103/64); // 103/64 <~ 1.618
		bank->reserve(cap);
	}
	do_string_resize_no_touch_memory(bank, sz);
}
#else
void string_resize_no_touch_memory(std::string* bank, size_t sz) {
  #if defined(_MSC_VER) && _MSC_VER >= 1938
    #if !_HAS_CXX23
	  #define resize_and_overwrite _Resize_and_overwrite
    #endif
	bank->resize_and_overwrite(sz, [](const char*, size_t n) { return n; });
  #elif defined(__cpp_lib_string_resize_and_overwrite)
	bank->resize_and_overwrite(sz, [](const char*, size_t n) { return n; });
  #else
	bank->resize(sz); // touch memory
  #endif
}
#endif

} // namespace terark

bool g_Terark_hasValgrind = terark::getEnvBool("Terark_hasValgrind", false);


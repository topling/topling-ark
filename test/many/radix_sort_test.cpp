#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#define _SCL_SECURE_NO_WARNINGS
#endif

#undef NDEBUG
#include <assert.h>

#include <stdio.h>
#include <string.h>
#include <string>
#include <algorithm>

#include <terark/radix_sort.hpp>
#include <terark/util/autoclose.hpp>
#include <terark/util/profiling.hpp>
//#include <obsoleted/wordseg/code_convert.hpp>
#include <terark/util/unicode_iterator.hpp>

#ifdef _MSC_VER
#define strcasecmp _stricmp
typedef _locale_t locale_t;
#endif

using namespace std;

struct Getpn
{
	template<class C>
	ptrdiff_t size(const std::basic_string<C>& x) const { return x.size()*sizeof(C); }

	template<class C>
	unsigned char* data(const std::basic_string<C>& x) const { return (unsigned char*)x.data(); }
};

bool suppress = false;

void printwc(const vector<string>& vs1)
{
	long cnt = 0;
	for (vector<string>::const_iterator i = vs1.begin(); i != vs1.end(); ++cnt)
	{
		vector<string>::const_iterator j;
		int cnt2 = 1;
		for (j = i + 1; j != vs1.end(); ++j)
		{
			if (*i == *j) ++cnt2;
			else break;
		}
		printf("%-30s %d\n", i->c_str(), cnt2);
		i = j;
	}
	printf("distinct_word_count=%ld\n", cnt);
}

void printwc2(const vector<string>& vs1, const vector<string>& vs2)
{
	printf("vs1 diff vs2++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
	for (size_t i = 0; i != vs1.size(); ++i)
	{
		if (vs1[i] != vs2[i])
			printf("%-40s %s\n", vs1[i].c_str(), vs2[i].c_str());
	}
	printf("vs1 diff vs2------------------------------------------------------\n");
}

#if 0
struct Holder
{
	char data[sizeof(string)];
};
#else
  // GCC c++11 string is not memmove-able
  using Holder = string;
#endif

/*
int codefun_toupper(const void* codetab, int code)
{
	return toupper(code);
}
*/
struct Comp_string
{
	bool operator()(const Holder& x, const Holder& y) const
	{
		const string& xs = (string&)x;
		const string& ys = (string&)y;
		return xs < ys;
	}
};
struct Comp_string_nocase
{
	bool operator()(const Holder& x, const Holder& y) const
	{
		const string& xs = (string&)x;
		const string& ys = (string&)y;
		return strcasecmp(xs.c_str(), ys.c_str()) < 0;
	}
};

struct GetChar {
	unsigned char operator()(const string& str, size_t idx) const {
		return str[idx];
	}
};
struct GetSize {
	size_t operator()(const string& str) const {
		return str.size();
	}
};

void test(vector<string> vs1, int radix, const unsigned char* codetab)
{
	vector<string> vs2 = vs1, vs3 = vs1;
	vector<string> vs4 = vs1;
	terark::profiling pf;
	long long t0 = pf.now();
	vector<Holder>& vsh = (vector<Holder>&)vs1;
	std::sort(vsh.begin(), vsh.end(), Comp_string());
	long long t1 = pf.now();
	if (!suppress) {
		printf("vs1----------------------------\n");
		printwc(vs1);
	}
	long long t2 = pf.now();
	terark::radix_sort(vs2.begin(), vs2.end(), Getpn(), codetab, radix);
	long long t3 = pf.now();
	vector<Holder>& vsh3 = (vector<Holder>&)vs3;
	std::stable_sort(vsh3.begin(), vsh3.end(), Comp_string());
	long long t4 = pf.now();
	terark::radix_sort_tpl(&vs4[0], vs4.size(), GetChar(), GetSize());
	long long t5 = pf.now();
	if (!suppress && vs1 != vs2) {
		printf("vs2----------------------------\n");
		printwc(vs2);
	}
	if (vs1 != vs2) printwc2(vs1, vs2);
	assert(vs1 == vs2);

	printf("sort[std=%f'ms, radix=%f'ms, stable=%f'ms, std/radix=%f, stable/radix=%f]: vs.size=%ld : %s empty str\n"
			, pf.us(t0, t1)/1e3
			, pf.us(t2, t3)/1e3
			, pf.us(t3, t4)/1e3
			, (double)pf.us(t0, t1)/pf.us(t2, t3)
			, (double)pf.us(t3, t4)/pf.us(t2, t3)
			, (long)vs1.size()
			, vs1[0].empty() ? "has" : "noo");
	assert(vs1 == vs4);
	printf("radix_sort_tpl: time=%f'ms\n", pf.mf(t4, t5));
}

void testw(vector<string> vs1, int radix, const unsigned char* codetab)
{
	vector<wstring> wvs1(vs1.size());
	ptrdiff_t i, n;
	for (i=0, n=vs1.size(); i < n; ++i) {
		//wvs1[i] = terark::wordseg::str2wcs(vs1[i]);
		wvs1[i].resize(0);
		terark::u8_to_u32_iterator<string::const_iterator>
		   	beg(vs1[i].begin()),
			end(vs1[i].end());
		std::copy(beg, end, back_inserter(wvs1[i]));
	}

	wchar_t wtab[256], *pwtab = NULL;
	if (codetab) {
		pwtab = wtab;
		for (i=0; i < 256; ++i) wtab[i] = codetab[i];
	}
	vector<wstring> wvs2(wvs1), wvs3(wvs1);

	terark::profiling pf;
	long long t0 = pf.now();
	std::sort(wvs1.begin(), wvs1.end());
	long long t1 = pf.now(), t2 = t1;
	terark::radix_sorter(wvs2.begin(), wvs2.end(), Getpn(), pwtab, radix).sort();
	long long t3 = pf.now();
	std::stable_sort(wvs3.begin(), wvs3.end());
	long long t4 = pf.now();

	assert(wvs1 == wvs2);
	printf("sort[std=%f'ms, radix=%f'ms, stable=%f'ms, std/radix=%f, stable/radix=%f]: vs.size=%ld : %s empty str\n"
			, pf.us(t0, t1)/1e3
			, pf.us(t2, t3)/1e3
			, pf.us(t3, t4)/1e3
			, (double)pf.us(t0, t1)/pf.us(t2, t3)
			, (double)pf.us(t3, t4)/pf.us(t2, t3)
			, (long)vs1.size()
			, vs1[0].empty() ? "has" : "noo");
}

int codefun_tolower(int ch, const void* loc)
{
//	return tolower_l(ch, loc);
	return tolower(ch);
}

void test_with_code_fun(vector<string> vs1, int radix)
{
	vector<string> vs2 = vs1;
	terark::profiling pf;

	long long t0 = pf.now();
	vector<Holder>& vsh = (vector<Holder>&)vs1;
	std::stable_sort(vsh.begin(), vsh.end(), Comp_string_nocase());
	long long t1 = pf.now();
	if (!suppress) {
		printf("vs1----------------------------\n");
		printwc(vs1);
	}
	long long t2 = pf.now();
	terark::radix_sort(vs2.begin(), vs2.end(), Getpn(), NULL, radix, codefun_tolower);
	long long t3 = pf.now();
	if (!suppress && vs1 != vs2) {
		printf("vs2----------------------------\n");
		printwc(vs2);
	}
	if (vs1 != vs2) printwc2(vs1, vs2);
	assert(vs1 == vs2);

	printf("sort[stable=%f'ms, radix=%f'ms, ratio=%f]: vs.size=%ld : %s empty str\n"
			, pf.us(t0, t1)/1e3, pf.us(t2, t3)/1e3
			, (double)pf.us(t0, t1)/pf.us(t2, t3)
			, (long)vs1.size()
			, vs1[0].empty() ? "has" : "noo");
}

int main(int argc, char **argv)
{
	vector<string> vs1;
	char word[512];
#if 0
	FILE* fp = fopen("/home/leipeng/terark/codelite/RadixSort/main.cpp", "r");
	if (NULL == fp) {
		fprintf(stderr, "can not open main.cpp\n");
		return 1;
	}
#else
	terark::Auto_fclose fp;
#endif

	unsigned char codetab[256] = {0}, *pcodetab = NULL;
	int radix = 'z' + 1;

	for (int ar = 1; ar < argc; ++ar) {
		if (strcmp(argv[ar], "suppress") == 0)
			suppress = true;
		else if (strcmp(argv[ar], "tab") == 0) {
			// sort ignore case, by translation table,
			// with radix = 53 (alpha + numeric + '_'),
			// all other chars are converted to '0'
			pcodetab = codetab;
			radix = 0;
			for (int c = '0'; c <= '9'; ++c) codetab[c] = (char)radix++;
			for (int c = 'A'; c <= 'Z'; ++c) codetab[c] = (char)radix++;
			codetab['_'] = (char)radix++;
			for (int c = 'a'; c <= 'z'; ++c) codetab[c] = (char)radix++;
		}
	}
	if (NULL == fp) {
		fprintf(stderr, "Reading from stdin...\n");
	}
	for (;;) {
		if (fscanf(fp.self_or(stdin), "%[_0-9A-Za-z]", word) == 1) {
			if (!suppress) printf("%s ", word);
			vs1.push_back(word);
		}
		else if (-1 == fgetc(fp.self_or(stdin)))
			break;
	}
	printf("\n");
	fflush(stdout);

	test(vs1, radix, pcodetab); // no empty key
	testw(vs1, radix, pcodetab);

	if (vs1.size() < 100*1000) {
	//	printf("test has empty key\n");
		vs1.push_back("");
		vs1.push_back("");
		test(vs1, radix, pcodetab);
	}
	printf("by codefun(tolower_l)\n");
	test_with_code_fun(vs1, radix);
	return 0;
}



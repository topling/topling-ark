#include <terark/hash_strmap.hpp>
#include <terark/util/linebuf.cpp>
#include <terark/util/autoclose.hpp>
#include <terark/util/profiling.cpp>
#include <stdio.h>
#include <errno.h>
#include <boost/current_function.hpp>

//#include <iostream>
#include <fstream>
#include <functional>
#include <map>
#include <vector>

const char delims[] = " \t:;-+&|%~!@#$%^&*()[]{}<>/?\\=,.'\"\r\n\b";

#ifdef __GNUC__
template<class ValuePlace, class CopyStrategy>
void wordcount_c(int argc, char** argv) {
	hash_strmap<long, fstring_func::hash, fstring_func::equal, ValuePlace, CopyStrategy> m;
	for (int i = 1; i < argc; ++i) {
		printf("%s:", argv[i]); fflush(stdout);
		terark::Auto_fclose fp(fopen(argv[i], "r"));
		if (NULL == fp) {
			fprintf(stderr, "can not open file: %s, for %s\n", argv[i], strerror(errno));
			continue;
		}
		terark::LineBuf line;
		long   lineno = 0;
		while (line.getline(fp) >= 0) {
			line.chomp();
			if (line.size() == 0) break;
			char* gp = line;
			char* tk = line;
			while (1) {
				tk = strsep(&gp, delims);
				if (tk) {
				//	printf("tklen=%ld, tk=%s^\n", long(gp?gp-tk-1:strlen(tk)), tk);
				   	m[tk]++;
				} else
					break;
		   	}
			++lineno;
		}
		printf("%8ld\n", lineno);
	}
	printf("===================================================\n");
	m.sort_by_value(std::greater<long>());
	printf("===================================================\n");
	for (size_t i = 0, n = m.size(); i < n; ++i)
		printf("%8ld\t0x%lX\t%ld\t%s\n", m.val(i), m.val(i), (long)m.key(i).n, m.key_c_str(i));
}
#endif

struct greater_second {
	template<class T>
	bool operator()(const T& x, const T& y) const {
		return x.second > y.second;
	}
	template<class T>
	bool operator()(long x, const T& y) const {	return x > y.second; }
	template<class T>
	bool operator()(const T& x, long y) const {	return x.second > y; }
};

typedef std::vector<std::pair<std::string, long> > wcv_t;

template<class HashStrMap>
wcv_t tovec(HashStrMap& m) {
//	printf("===================================================\n");
//	m.sort_by_value(std::greater<long>());
	m.sort_v2k2(std::greater<long>());
//	printf("===================================================\n");
	HashStrMap m1 = m, m2 = m;
	terark::profiling pf;
	long long t0 = pf.now();
	m1.sort_slow();
	long long t1 = pf.now();
	m2.sort_fast();
	long long t2 = pf.now();
	assert(m1.size() == m2.size());
	printf("m1.size=%zd, sort_slow=%f'ms sort_fast=%f'ms\n", m1.size(), pf.mf(t0,t1), pf.mf(t1,t2));
	for (ssize_t i = 0, n = m1.size(); i < n; ++i) {
		fstring s1 = m1.key(i);
		fstring s2 = m2.key(i);
		assert(strcmp(s1.p, s2.p) == 0);
		long c1 = m1.val(i);
		long c2 = m2.val(i);
		assert(c1 == c2);
		size_t idx1 = m1.find_i(s1);
		size_t idx2 = m2.find_i(s2);
		assert(idx1 == idx2);
		std::string ss = std::string(s1.p, s1.n) + "abcd";
		size_t jdx1 = m1.find_i(ss);
		size_t jdx2 = m2.find_i(ss);
		if (jdx1 != m1.size()) {
			printf("ss=%s, found=%s\n", ss.c_str(), m1.key(jdx1).p);
			assert(jdx2 == jdx1);
		} else
			assert(jdx2 == m2.size());
	}
	wcv_t res;
	res.reserve(m.size());
	for (size_t i = 0, n = m.size(); i < n; ++i) {
		fstring k = m.key(i);
		res.push_back(std::make_pair(k.str(), m.val(i)));
#if 0
		printf("%8ld\t0x%lX\t%ld\t%s\n", m.val(i), m.val(i), k.n, k.p);
#endif
	}
	for (size_t i = 0, n = m.size(); i < n; ) {
		long val = res[i].second;
		size_t j = m.upper_bound_by_value(val, std::greater<long>());
		size_t k = std::upper_bound(res.begin(), res.end(), val, greater_second()) - res.begin();
		assert(k == j);
		j = m.lower_bound_by_value(val, std::greater<long>());
		assert(i == j);
		std::pair<wcv_t::iterator, wcv_t::iterator> ii =
		std::equal_range(res.begin(), res.end(), val, greater_second());
		std::pair<size_t, size_t> ii1, ii2;
		ii1.first  = ii.first  - res.begin();
		ii1.second = ii.second - res.begin();
		ii2 = m.equal_range_by_value(val, std::greater<long>());
		assert(ii1 == ii2);
		i = ii1.second;
	}
	return res;
}

struct greater_second_less_first {
	template<class T>
	bool operator()(const T& x, const T& y) const {
		if (x.second > y.second)
			return true;
		else if (x.second < y.second)
			return false;
		else
			return x.first < y.first;
	}
	template<class T>
	bool operator()(long x, const T& y) const {	return x > y.second; }
	template<class T>
	bool operator()(const T& x, long y) const {	return x.second > y; }
};

wcv_t tovec(std::map<std::string, long>& m) {
	wcv_t res;
	res.reserve(m.size());
	std::copy(m.begin(), m.end(), std::back_inserter(res));
	std::sort(res.begin(), res.end(), greater_second_less_first());
#if 0
	for (ssize_t j = 0, n = m.size(); j < n; ++j) {
		printf("%8ld\t%ld\t%s\n", res[j].second,
			(long)res[j].first.size(), res[j].first.c_str());
	}
#endif
	return res;
}

template<class Map>
void wordcount0(Map& m, int argc, char** argv) {
	printf("===================================================\n");
	printf("%s\n", BOOST_CURRENT_FUNCTION);
	printf("---------------------------------------------------\n");
	for (int i = 1; i < argc; ++i) {
		printf("%s:", argv[i]); fflush(stdout);
		std::ifstream ifs(argv[i]);
		if (!ifs.is_open()) {
			fprintf(stderr, "can not open file: %s\n", argv[i]);
			continue;
		}
		std::string line;
		long lineno = 0;
		while (!ifs.eof() && std::getline(ifs, line)) {
			const char* tk = strtok(&line[0], delims);
			while (tk) {
			   	m[tk]++;
				tk = strtok(NULL, delims);
			}
			++lineno;
		}
		printf("%8ld\n", lineno);
	}
}

template<class ValuePlace, class CopyStrategy>
wcv_t wordcount(int argc, char** argv) {
	hash_strmap<long, fstring_func::hash, fstring_func::equal, ValuePlace, CopyStrategy> m;
	wordcount0(m, argc, argv);
	return tovec(m);
}
wcv_t wordcount(int argc, char** argv) {
	std::map<std::string, long> m;
	wordcount0(m, argc, argv);
	return tovec(m);
}

void check_eq(wcv_t& v1, wcv_t& v2) {
	assert(v1.size() == v2.size());
	for(size_t i = 0, n = v1.size(); i < n; ++i) {
		assert(v1[i].first == v2[i].first);
	}
}

int main(int argc, char** argv) {
	printf("SP_ALIGN=%d\n", SP_ALIGN);
	wcv_t v00 = wordcount<ValueInline, FastCopy>(argc, argv);
	wcv_t v01 = wordcount<ValueInline, SafeCopy>(argc, argv);
	wcv_t v10 = wordcount<ValueOut   , FastCopy>(argc, argv);
	wcv_t v11 = wordcount<ValueOut   , SafeCopy>(argc, argv);
	wcv_t vmm = wordcount(argc, argv); // std::map<string, long>
	check_eq(vmm, v00);
	check_eq(vmm, v01);
	check_eq(vmm, v10);
	check_eq(vmm, v11);
	return 0;
}


#include <terark/util/strjoin.hpp>
#include <terark/util/profiling.cpp>
#include <iostream>

using namespace std;

long c_temp = 0;

struct MyStr : std::string {
	MyStr(const char* s) : std::string(s) { c_temp++; }
	MyStr(const char* s, ptrdiff_t n) : std::string(s,n ) { c_temp++; }
	MyStr(const MyStr& y) : std::string(y) { c_temp++; }
	MyStr& operator+=(const char* s) { this->append(s); return *this; }
	MyStr  operator+(const char* s) { MyStr t(*this); t.append(s); return t; }
};

void fast(int c) {
	MyStr a("");
	MyStr empty("");
	for (int i = 0; i < c; ++i)
		a = strjoin(empty) + "0" + "1" + "2" + "3" + "4" + "5" + "6" + "7" + "8" + "9";
}

void slow(int c) {
	MyStr a("");
	MyStr empty("");
	for (int i = 0; i < c; ++i)
		a = empty + "0" + "1" + "2" + "3" + "4" + "5" + "6" + "7" + "8" + "9";
}

void foo() {
	string a = "123_" + strjoin("") + "A" + "B" + strjoin("abc") + strjoin("012345", 4);
	cout << a << endl;
}

int main() {
	int loop = 1000000;
	terark::profiling pf;
	long t0 = pf.now();
	long c0 = c_temp;
	fast(loop);
	long c1 = c_temp;
	long t1 = pf.now();
	slow(loop);
	long c2 = c_temp;
	long t2 = pf.now();
	printf("loop=%d: fast[temp_cnt=%ld time=%f] slow[temp_cnt=%ld time=%f]\n", loop, c1-c0, pf.sf(t0,t1), c2-c1, pf.sf(t1,t2));
	return 0;
}


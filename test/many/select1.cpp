#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

unsigned left_select1(uint64_t v, unsigned r) {
//  uint64_t v = v0;     // Input value to find position with rank r.
//	unsigned r = pc-r0;  // Input: bit's desired rank [1-64].
    unsigned s;          // Output: Resulting position of bit with rank r [1-64]
    uint64_t a, b, c, d; // Intermediate temporaries for bit count.
    unsigned t;          // Bit count temporary.
    uint64_t const neg1 = uint64_t(-1);

    // Do a normal parallel bit count for a 64-bit integer,
    // but store all intermediate steps.
    // a = (v & 0x5555...) + ((v >> 1) & 0x5555...);
    a =  v - ((v >> 1) & neg1/3);
    // b = (a & 0x3333...) + ((a >> 2) & 0x3333...);
    b = (a & neg1/5) + ((a >> 2) & neg1/5);
    // c = (b & 0x0f0f...) + ((b >> 4) & 0x0f0f...);
    c = (b + (b >> 4)) & neg1/0x11;
    // d = (c & 0x00ff...) + ((c >> 8) & 0x00ff...);
    d = (c + (c >> 8)) & neg1/0x101;
    t = (d >> 32) + (d >> 48);
    // Now do branchless select!
    s  = 64;
    // if (r > t) {s -= 32; r -= t;}
    s -= ((t - r) & 256) >> 3; r -= (t & ((t - r) >> 8));
    t  = (d >> (s - 16)) & 0xff;
    // if (r > t) {s -= 16; r -= t;}
    s -= ((t - r) & 256) >> 4; r -= (t & ((t - r) >> 8));
    t  = (c >> (s - 8)) & 0xf;
    // if (r > t) {s -= 8; r -= t;}
    s -= ((t - r) & 256) >> 5; r -= (t & ((t - r) >> 8));
    t  = (b >> (s - 4)) & 0x7;
    // if (r > t) {s -= 4; r -= t;}
    s -= ((t - r) & 256) >> 6; r -= (t & ((t - r) >> 8));
    t  = (a >> (s - 2)) & 0x3;
    // if (r > t) {s -= 2; r -= t;}
    s -= ((t - r) & 256) >> 7; r -= (t & ((t - r) >> 8));
    t  = (v >> (s - 1)) & 0x1;
    // if (r > t) s--;
    s -= ((t - r) & 256) >> 8;
	return 65 - s;
}

template<class Uint>
unsigned slow_select1(Uint x, unsigned r) {
	enum { UintBits = sizeof(Uint) * 8 };
	unsigned hit = 0;
	Uint bm = x;
	for(unsigned j = 0; j < UintBits; ++j) {
		if (hit == r) return j;
		if (bm & 1) hit++;
		bm >>= 1;
	}
	assert(hit == r);
	return UintBits;
}

unsigned fast_select1(uint64_t v, unsigned r) {
	if (0 == r) return 0;
//	unsigned p = fast_popcount(x); assert(r <= p);
//	uint64_t v = x;      // Input value to find position with rank r.
//	unsigned r = r;      // Input: bit's desired rank [1-64].
	unsigned s;          // Output: Resulting position of bit with rank r [1-64]
	uint64_t a, b, c, d; // Intermediate temporaries for bit count.
	unsigned t;          // Bit count temporary.

	// Do a normal parallel bit count for a 64-bit integer,
	// but store all intermediate steps.
	a =  v - ((v >> 1) & 0x5555555555555555);
	b = (a & 0x3333333333333333) + ((a >> 2) & 0x3333333333333333);
	c = (b + (b >> 4)) & 0x0F0F0F0F0F0F0F0F;
	d = (c + (c >> 8)) & 0x00FF00FF00FF00FF;
//	t = (d >> 32) + (d >> 48); // popcnt(hi32)
	t = ((d >> 16) + d) & 255; // popcnt(lo32)
//	printf("v=%016lX  t=%d  popcnt(lo32)=%d\n", v, t, __builtin_popcount(v));
	s = 0;
	if (r > t) {s += 32; r -= t;}

	t = (d >> s) & 0xFF; //printf("t=%d popcnt16(v>>%d)=%d\n", t, s, __builtin_popcount(uint16_t(v>>s)));
	if (r > t) {s += 16; r -= t;}

	t = (c >> s) & 0xF; //printf("t=%d popcnt8(v>>%d)=%d\n", t, s, __builtin_popcount(v>>s & 255));
	if (r > t) {s += 8; r -= t;}

	t = (b >> s) & 0x7; //printf("t=%d popcnt4(v>>%d)=%d\n", t, s, __builtin_popcount(v>>s & 15));
	if (r > t) {s += 4; r -= t;}

	t = (a >> s) & 0x3;
	if (r > t) {s += 2; r -= t;}

	t = (v >> s) & 0x1;
//	if (r > t) s++;
	if (r > t) {s += 1; r -= t;}

//	assert(r == 0);

	return s+1;
}

int main(int argc, char* argv[1]) {
//	uint64_t v0 = strtoull(argv[1], NULL, 0);
//	unsigned r0 = atoi(argv[2]);
//	unsigned pc = __builtin_popcountl(v0);
//	printf("left_select1(%lX, %u) = %u\n", v0, r0, s-1);
	int loop = 1;
	if (argc >= 2) loop = atoi(argv[1]);
	typedef unsigned long long ullong;
	for (int i = 0; i < loop; ++i) {
		ullong   v = rand()|(ullong)rand()<<33;
		unsigned p = __builtin_popcountll(v);
		unsigned r = v % (p+1);
		unsigned f = fast_select1(v, r);
		unsigned s = slow_select1(v, r);
		if (f != s) {
			printf("i=%3d v=%016llX pc=%2d r=%2d fast=%2u slow=%2u\n", i, v, p, r, f, s);
		}
	}
	return 0;
}


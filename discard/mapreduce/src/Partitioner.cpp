#include "Partitioner.h"
#include <stddef.h>

Partitioner::~Partitioner()
{
}

int Partitioner::hash(const void* key, int klen, int npart)
{
	const unsigned char* b = (const unsigned char*)key;
	unsigned long long h = 23723891;
	for (int i = 0; i < klen; ++i)
	{
		h = (h << 3 | h >> (8*sizeof(h) - 3));
		h ^= b[i];
	}
	return h % npart;
}


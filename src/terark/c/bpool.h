#ifndef __terark_c_mpool_h__
#define __terark_c_mpool_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
# pragma warning(disable: 4127)
#endif

#include "config.h"
#include <stddef.h> // for size_t

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned uint32_t;

/** 32K */
#define BB_CHUNK_SIZE  0x8000

struct bchunk_entry
{
	char*    base;
	uint32_t used;
	uint32_t head;
};

struct ballocator
{
	uint32_t (*balloc)(struct ballocator* self);
	void     (*bfree )(struct ballocator* self, uint32_t id);
	struct bchunk_entry* pChunks;
	struct bchunk_entry* recent;
	int free_slot;
	int nChunks;
	int nMaxChunks;
	uint32_t cell_size;
	uint32_t nUsed_cell;
};

TERARK_DLL_EXPORT int bbinit(struct ballocator* self, uint32_t cell_size);
TERARK_DLL_EXPORT void bbdestroy(struct ballocator* self);

TERARK_DLL_EXPORT uint32_t bballoc(struct ballocator* self);
TERARK_DLL_EXPORT void bbfree(struct ballocator* self, uint32_t id);

TERARK_DLL_EXPORT void* bbaddress(struct ballocator* self, uint32_t id);

#define BB_ADDRESS(self, id) (self->pChunks[(id)/BB_CHUNK_SIZE].base + self->cell_size * ((id)%BB_CHUNK_SIZE))

#ifdef __cplusplus
}
#endif

#endif // __terark_c_mpool_h__



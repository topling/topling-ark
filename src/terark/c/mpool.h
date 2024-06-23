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

//------------------------------------------------------------------------------------------
struct sallocator
{
	void* (*salloc  )(struct sallocator* self, size_t size);
	void  (*sfree   )(struct sallocator* self, void* block, size_t size);
	void* (*srealloc)(struct sallocator* self, void* block, size_t old_size, size_t new_size);
};
typedef void* (*salloc_ft  )(struct sallocator* self, size_t size);
typedef	void  (*sfree_ft   )(struct sallocator* self, void* block, size_t size);
typedef void* (*srealloc_ft)(struct sallocator* self, void* block, size_t old_size, size_t new_size);

struct fixed_mpool
{
	struct sallocator*  sa;
	struct mpool_chunk* pChunks;
	struct mpool_cell*  head;
	size_t iNextChunk;
	size_t nChunks;
	size_t cell_size;
	size_t chunk_size;
	size_t used_cells;
};

struct sfixed_mpool
{
//------------------------------------------------------------------------------------------
/// export a sallocator interface
	void* (*salloc  )(struct sallocator* self, size_t size);
	void  (*sfree   )(struct sallocator* self, void* block, size_t size);
	void* (*srealloc)(struct sallocator* self, void* block, size_t old_size, size_t new_size);
//------------------------------------------------------------------------------------------
	struct fixed_mpool fmp;
};

struct big_block_header
{
	struct big_block_header *next, *prev;
	size_t size;
	size_t reserved; // for alignment
};

struct mpool
{
//------------------------------------------------------------------------------------------
/// export a sallocator interface
	void* (*salloc  )(struct sallocator* self, size_t size);
	void  (*sfree   )(struct sallocator* self, void* block, size_t size);
	void* (*srealloc)(struct sallocator* self, void* block, size_t old_size, size_t new_size);
//------------------------------------------------------------------------------------------

	/// sallocator for this mpool self
	struct sallocator* sa;

	struct fixed_mpool* fixed;
	size_t max_cell_size;
	size_t chunk_size;

/// 是否允许 mpool 分配超出 max_cell_size 的内存块
/// allow alloc memory block bigger than max_cell_size
#define TERARK_MPOOL_ALLOW_BIG_BLOCK	1

/// when destroy, auto free big block or not
#define TERARK_MPOOL_AUTO_FREE_BIG		2

/// use malloc or this->salloc to alloc big block
#define TERARK_MPOOL_MALLOC_BIG			4

	size_t big_flags;
	size_t big_blocks; // size > max_cell_size, use malloc, this is rare case
	struct big_block_header big_list;
};

/***********************************************************************/
TERARK_DLL_EXPORT
void*
default_srealloc(struct sallocator* sa, void* ptr, size_t old_size, size_t new_size);
/***********************************************************************/


/***********************************************************************/
TERARK_DLL_EXPORT void fixed_mpool_init   (struct fixed_mpool* mpf);
TERARK_DLL_EXPORT void fixed_mpool_destroy(struct fixed_mpool* mpf);

TERARK_DLL_EXPORT void* fixed_mpool_alloc(struct fixed_mpool* mpf);
TERARK_DLL_EXPORT void  fixed_mpool_free (struct fixed_mpool* mpf, void* ptr);
/***********************************************************************/


/***********************************************************************/
/**
 * sfixed_mpool_{salloc|sfree} should only called by sallocator interface
 * sfixed_mpool_srealloc is an assert only hook.
 */
TERARK_DLL_EXPORT void sfixed_mpool_init   (struct sfixed_mpool* mp);
TERARK_DLL_EXPORT void sfixed_mpool_destroy(struct sfixed_mpool* mp);
/***********************************************************************/


/***********************************************************************/
/**
 * mpool_{salloc|sfree} may called by function name, or by interface
 */
TERARK_DLL_EXPORT void mpool_init   (struct mpool* mp);
TERARK_DLL_EXPORT void mpool_destroy(struct mpool* mp);

TERARK_DLL_EXPORT void* mpool_salloc(struct mpool* mp, size_t size);
TERARK_DLL_EXPORT void  mpool_sfree (struct mpool* mp, void* ptr, size_t size);

TERARK_DLL_EXPORT size_t mpool_used_cells(const struct mpool* mp);
TERARK_DLL_EXPORT size_t mpool_used_bytes(const struct mpool* mp);
/***********************************************************************/


/***********************************************************************/
TERARK_DLL_EXPORT struct mpool* mpool_get_global(void);

TERARK_DLL_EXPORT void* gsalloc(size_t size);
TERARK_DLL_EXPORT void gsfree(void* ptr, size_t size);
/***********************************************************************/

#ifdef __cplusplus
}
#endif

#endif // __terark_c_mpool_h__



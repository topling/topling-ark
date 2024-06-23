#include "mpool.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#ifdef _MSC_VER
/* Always compile this module for speed, not size */
#pragma optimize("t", on)
#endif

#ifdef _MSC_VER
#  define FZD "%Id"
#else
#  define FZD "%zd"
#endif

/* must be power of 2 and greater than sizeof(void*) */
#define MPOOL_MIN_CELL 8

#define MPOOL_MIN_CHUNK 256

struct mpool_cell
{
	struct mpool_cell* next;
};

struct mpool_chunk
{
	struct mpool_cell* cell; // cell array
	size_t size; // size in bytes;
};

/**********************************************************************************/
/**
 * sallocator use malloc/free
 */
static void*
malloc_salloc(struct sallocator* sa, size_t size)
{
	return malloc(size);
}

static void
malloc_sfree(struct sallocator* sa, void* block, size_t size)
{
	free(block);
}

static void*
malloc_srealloc(struct sallocator* sa, void* block, size_t old_size, size_t new_size)
{
	return realloc(block, new_size);
}
/**********************************************************************************/

void*
default_srealloc(struct sallocator* sa, void* ptr, size_t old_size, size_t new_size)
{
	void* q = sa->salloc(sa, new_size);
	assert(old_size > 0);
	assert(new_size > 0);
	if (NULL == q) return NULL;
	memcpy(q, ptr, old_size < new_size ? old_size : new_size);
	sa->sfree(sa, ptr, old_size);
	return q;
}

static void
chunk_init(struct mpool_cell* cell, size_t cell_count, size_t cell_size)
{
	size_t i;
	struct mpool_cell* p = cell;
	assert(cell_size % MPOOL_MIN_CELL == 0);
	for (i = 0; i < cell_count-1; ++i)
		p = p->next = (struct mpool_cell*)((char*)p + cell_size);
	p->next = NULL;
}

/**********************************************************************************/
static struct sallocator fal = {
	&malloc_salloc,
	&malloc_sfree,
	&malloc_srealloc
};

/**********************************************************************************/
static void* sfixed_mpool_salloc  (struct sfixed_mpool* fmp, size_t size);
static void* sfixed_mpool_srealloc(struct sfixed_mpool* fmp, size_t old_size, size_t new_size);
static void  sfixed_mpool_sfree   (struct sfixed_mpool* fmp, void* ptr, size_t size);

/**
 * require initialized fields:
 *	 cell_size
 *	 size
 *	 sa [0 OR initialized]
 */
void fixed_mpool_init(struct fixed_mpool* fmp)
{
	if (NULL == fmp->sa)
		fmp->sa = &fal;
	else {
		assert(NULL != fmp->sa->salloc);
		assert(NULL != fmp->sa->sfree);
		if (NULL == fmp->sa->srealloc)
			fmp->sa->srealloc = &default_srealloc;
	}
	assert(fmp->chunk_size > 0);
	assert(fmp->cell_size > 0);
	assert(fmp->cell_size < fmp->chunk_size);

	fmp->cell_size = (fmp->cell_size + MPOOL_MIN_CELL - 1) / MPOOL_MIN_CELL * MPOOL_MIN_CELL;
	fmp->chunk_size = (fmp->chunk_size + MPOOL_MIN_CHUNK - 1) / MPOOL_MIN_CHUNK * MPOOL_MIN_CHUNK;

	if (fmp->nChunks < MPOOL_MIN_CHUNK/sizeof(struct mpool_chunk))
		fmp->nChunks = MPOOL_MIN_CHUNK/sizeof(struct mpool_chunk);

	fmp->iNextChunk = 0;
	fmp->pChunks = NULL;
	fmp->head = NULL;
	fmp->used_cells = 0;
}

void sfixed_mpool_init(struct sfixed_mpool* sfmp)
{
	fixed_mpool_init(&sfmp->fmp);
	sfmp->salloc   = (salloc_ft  )&sfixed_mpool_salloc;
	sfmp->sfree    = (sfree_ft   )&sfixed_mpool_sfree;
	sfmp->srealloc = (srealloc_ft)&sfixed_mpool_srealloc;
}

void fixed_mpool_destroy(struct fixed_mpool* fmp)
{
	if (fmp->pChunks) {
		struct sallocator* sa = fmp->sa;
		ptrdiff_t i;
		for (i = fmp->iNextChunk - 1; i >= 0; --i)
			sa->sfree(sa, fmp->pChunks[i].cell, fmp->pChunks[i].size);
		sa->sfree(sa, fmp->pChunks, sizeof(struct mpool_chunk) * fmp->nChunks);

		fmp->iNextChunk = 0;
		fmp->pChunks = 0;
		fmp->head = NULL;
		fmp->used_cells = 0;
	//	memset(fmp, 0, sizeof(struct fixed_mpool));
	} else {
	//	fprintf(stderr, "warning: fixed_mpool_destroy: already destroyed or not inited");
	}
}

void sfixed_mpool_destroy(struct sfixed_mpool* sfmp)
{
	fixed_mpool_destroy(&sfmp->fmp);
}

struct mpool_cell* fixed_mpool_add_chunk(struct fixed_mpool* fmp)
{
	struct mpool_cell* cell;

	if (terark_unlikely(NULL == fmp->pChunks)) {
		size_t size = sizeof(struct mpool_chunk) * fmp->nChunks;
		fmp->pChunks = (struct mpool_chunk*)fmp->sa->salloc(fmp->sa, size);
		if (NULL == fmp->pChunks)
			return NULL;
	} else if (terark_unlikely(fmp->iNextChunk == fmp->nChunks)) {
		size_t old_size = sizeof(struct mpool_chunk) * fmp->nChunks;
		size_t new_size = 2 * old_size;

		// allocate new chunk array
		struct mpool_chunk* c = (struct mpool_chunk*)
			fmp->sa->srealloc(fmp->sa, fmp->pChunks, old_size, new_size);

		if (NULL == c) return NULL;
		fmp->pChunks = c;
		fmp->nChunks *= 2;     // chunk array expanded 2
		fmp->chunk_size *= 2;  // chunk_size  expanded 2 also
	}

	// allocate a new cell array
	cell = (struct mpool_cell*)fmp->sa->salloc(fmp->sa, fmp->chunk_size);
	if (NULL == cell) return NULL;
	fmp->pChunks[fmp->iNextChunk].cell = cell;
	fmp->pChunks[fmp->iNextChunk].size = fmp->chunk_size;
	fmp->iNextChunk++;
	chunk_init(cell, fmp->chunk_size / fmp->cell_size, fmp->cell_size);

	/* alloc cell */
	fmp->used_cells++;
	fmp->head = cell->next;

	return cell;
}

#define FIXED_MPOOL_IMPL_ALLOC(fmp) 	\
{										\
	struct mpool_cell* cell = fmp->head;\
	if (terark_likely(NULL != cell)) {	\
		fmp->used_cells++;				\
		fmp->head = cell->next;			\
		return cell;					\
	}									\
	return fixed_mpool_add_chunk(fmp);	\
}
/***************************************************************/

#define FIXED_MPOOL_IMPL_FREE(fmp, ptr)	\
{										\
	struct mpool_cell* cell = (struct mpool_cell*)ptr; \
	cell->next = fmp->head;	\
	fmp->used_cells--;		\
	fmp->head = cell;		\
}
/***************************************************************/
void*
fixed_mpool_alloc(struct fixed_mpool* fmp)
FIXED_MPOOL_IMPL_ALLOC(fmp)

void*
sfixed_mpool_salloc(struct sfixed_mpool* sfmp, size_t size)
{
	if (terark_unlikely(size > sfmp->fmp.cell_size)) {
		fprintf(stderr, "fatal: sfixed_mpool_salloc:[cell_size="FZD", request_size="FZD"]\n", sfmp->fmp.cell_size, size);
		abort();
	}
	FIXED_MPOOL_IMPL_ALLOC((&sfmp->fmp))
}

void
fixed_mpool_free(struct fixed_mpool* fmp, void* ptr)
FIXED_MPOOL_IMPL_FREE(fmp, ptr)

void
sfixed_mpool_sfree(struct sfixed_mpool* sfmp, void* ptr, size_t size)
{
	if (terark_unlikely(size > sfmp->fmp.cell_size)) {
		fprintf(stderr, "fatal: sfixed_mpool_sfree:[cell_size="FZD", request_size="FZD"]\n", sfmp->fmp.cell_size, size);
		abort();
	}
	FIXED_MPOOL_IMPL_FREE((&sfmp->fmp), ptr)
}

static void*
sfixed_mpool_srealloc(struct sfixed_mpool* fmp, size_t old_size, size_t new_size)
{
	fprintf(stderr, "fatal: sfixed_mpool_srealloc: this function should not be called\n");
	abort();
	return NULL; // avoid warning
}

/**********************************************************************************/
/**
 * require initialized fields:
 *   max_cell_size
 *	 size
 *	 sa [0 OR initialized]
 */
void mpool_init(struct mpool* mp)
{
	size_t i, nFixed;
	struct sallocator* al;

	assert(mp->max_cell_size < mp->chunk_size);

	if (NULL == mp->salloc)
		al = mp->sa = &fal;
	else {
		al = mp->sa;
		assert(NULL != al->salloc);
		assert(NULL != al->sfree);
		if (NULL == al->srealloc)
			al->srealloc = &default_srealloc;
	}
	mp->salloc = (salloc_ft)&mpool_salloc;
	mp->sfree = (sfree_ft)&mpool_sfree;
	mp->srealloc = (srealloc_ft)&default_srealloc;

	mp->max_cell_size = (mp->max_cell_size + MPOOL_MIN_CELL - 1) / MPOOL_MIN_CELL * MPOOL_MIN_CELL;
	mp->chunk_size = (mp->chunk_size + MPOOL_MIN_CHUNK - 1) / MPOOL_MIN_CHUNK * MPOOL_MIN_CHUNK;
	nFixed = mp->max_cell_size / MPOOL_MIN_CELL;

	mp->fixed = (struct fixed_mpool*)al->salloc(al, sizeof(struct fixed_mpool) * nFixed);
	if (NULL == mp->fixed) {
		fprintf(stderr, "fatal: terark.mpool_init[max_cell_size="FZD", size="FZD"]\n"
				, mp->max_cell_size, mp->chunk_size);
		abort();
	}

	for (i = 0; i < nFixed; ++i)
	{
		mp->fixed[i].cell_size = (i + 1) * MPOOL_MIN_CELL;
		mp->fixed[i].chunk_size = mp->chunk_size;
		mp->fixed[i].nChunks = 16;
		mp->fixed[i].sa = mp->sa;
		fixed_mpool_init(&mp->fixed[i]);
	}
//	if (mp->big_flags & TERARK_MPOOL_ALLOW_BIG_BLOCK) {
//	always init
		mp->big_blocks = 0;
		mp->big_list.prev = mp->big_list.next = &mp->big_list;
		mp->big_list.size = 0;
		mp->big_list.reserved = 0;
//	}
}

void mpool_destroy(struct mpool* mp)
{
	size_t i, nFixed;
	if (NULL == mp->fixed) {
		fprintf(stderr, "fatal: terark.mpool_destroy: not inited or has already destroyed\n");
		return;
	}
	if (mp->big_flags & TERARK_MPOOL_ALLOW_BIG_BLOCK)
	{
		if (mp->big_flags & TERARK_MPOOL_AUTO_FREE_BIG)
		{
			size_t total_size = 0;
			struct big_block_header *p;
			for (i = 0, p = mp->big_list.next; p != &mp->big_list; ++i)
			{
				struct big_block_header *q = p->next;
				total_size += p->size;
				if (mp->big_flags & TERARK_MPOOL_MALLOC_BIG)
					free(p);
				else
					mp->sa->sfree(mp->sa, p, p->size);
				p = q;
			}
			if (i != mp->big_blocks || total_size != mp->big_list.size)
			{
				fprintf(stderr
					, "fatal: terark.mpool_destroy: bad track list, big_blocks="FZD", i="FZD"\n"
					, mp->big_blocks, i
					);
			}
		} else {
			if (mp->big_blocks)
				fprintf(stderr
					, "warning: mpool_destroy: memory leak big blocks="FZD"\n"
					, mp->big_blocks
					);
		}
	}
	nFixed = mp->max_cell_size / MPOOL_MIN_CELL;
	for (i = 0; i < nFixed; ++i)
		fixed_mpool_destroy(&mp->fixed[i]);
	mp->sa->sfree(mp->sa, mp->fixed, sizeof(struct fixed_mpool) * nFixed);

	mp->fixed = NULL;
}

static
void* mpool_salloc_big(struct mpool* mp, size_t size)
{
	if (mp->big_flags & TERARK_MPOOL_ALLOW_BIG_BLOCK)
   	{
		// this is rare case
		struct big_block_header *p, *h;

		if (mp->big_flags & TERARK_MPOOL_AUTO_FREE_BIG)
			size += sizeof(struct big_block_header);
		p = (struct big_block_header*)
			( mp->big_flags & TERARK_MPOOL_MALLOC_BIG
			? malloc(size)
			: mp->sa->salloc(mp->sa, size)
			);
		if (p) {
			if (mp->big_flags & TERARK_MPOOL_AUTO_FREE_BIG) {
				h = &mp->big_list;
				p->size = size;
				p->prev = h;
				p->next = h->next;
				h->next->prev = p;
				h->next = p;

				h->size += size; // accumulate size in list header
				mp->big_blocks++;
				return (p + 1);
			} else
				return p;
		} else
			return NULL;
	} else {
		fprintf(stderr, "fatal: mpool_salloc: [size="FZD", max_cell_size="FZD"]\n", size, mp->max_cell_size);
		abort();
	}
	assert(0);
	return NULL; // avoid warnings
}

static
void mpool_sfree_big(struct mpool* mp, void* ptr, size_t size)
{
	if (mp->big_flags & TERARK_MPOOL_ALLOW_BIG_BLOCK)
   	{
		if (mp->big_flags & TERARK_MPOOL_AUTO_FREE_BIG)
	   	{
			// this is rare case
			struct big_block_header* bbh = (struct big_block_header*)ptr - 1;
			bbh->prev->next = bbh->next;
			bbh->next->prev = bbh->prev;
			if (size + sizeof(struct big_block_header) != bbh->size) {
				fprintf(stderr, "fatal: mpool_sfree: size_error[recored="FZD", passed="FZD"]\n"
						, bbh->size, size + sizeof(struct big_block_header));
			}
			size = bbh->size;
			ptr = bbh;
			mp->big_list.size -= size; // accumulate size in list header
		}
		mp->big_blocks--;

		if (mp->big_flags & TERARK_MPOOL_MALLOC_BIG)
			free(ptr);
		else
			mp->sa->sfree(mp->sa, ptr, size);
	} else {
		fprintf(stderr, "fatal: mpool_sfree: [size="FZD", max_cell_size="FZD"]\n", size, mp->max_cell_size);
		abort();
	}
	return;
}

void*
mpool_salloc(struct mpool* mp, size_t size)
{
	assert(size > 0);
	if (terark_likely(size <= mp->max_cell_size)) {
		size_t idx = (size - 1) / MPOOL_MIN_CELL;
		struct fixed_mpool* fmp = &mp->fixed[idx];
		FIXED_MPOOL_IMPL_ALLOC(fmp)
	} else
		return mpool_salloc_big(mp, size);
}

void
mpool_sfree(struct mpool* mp, void* ptr, size_t size)
{
	assert(size > 0);
	if (terark_likely(size <= mp->max_cell_size)) {
		size_t idx = (size - 1) / MPOOL_MIN_CELL;
		struct fixed_mpool* fmp = &mp->fixed[idx];
		FIXED_MPOOL_IMPL_FREE(fmp, ptr)
	} else
		mpool_sfree_big(mp, ptr, size);
}

size_t mpool_used_cells(const struct mpool* mp)
{
	size_t i, n = mp->max_cell_size / MPOOL_MIN_CELL;
	size_t used = 0;
	for (i = 0; i < n; ++i)
		used += mp->fixed[i].used_cells;
	return used;
}

size_t mpool_used_bytes(const struct mpool* mp)
{
	size_t i, n = mp->max_cell_size / MPOOL_MIN_CELL;
	size_t used = 0;
	for (i = 0; i < n; ++i)
		used += mp->fixed[i].cell_size * mp->fixed[i].used_cells;
	return used;
}

static struct mpool global_mpool;
static void destroy_global_mpool(void)
{
	if (global_mpool.fixed) {
		size_t used = mpool_used_cells(&global_mpool);
		if (used) {
			fprintf(stderr, "warning: memory leak in global_mpool\n");
		}
		mpool_destroy(&global_mpool);
	}
}

struct mpool* mpool_get_global(void)
{
	if (NULL == global_mpool.fixed) {
		global_mpool.chunk_size = 4096;
		global_mpool.max_cell_size = 256;
		global_mpool.sa = &fal;
		global_mpool.big_flags = 0
			|TERARK_MPOOL_ALLOW_BIG_BLOCK
			|TERARK_MPOOL_AUTO_FREE_BIG
			|TERARK_MPOOL_MALLOC_BIG
			;
		mpool_init(&global_mpool);
		atexit(&destroy_global_mpool);
	}
	return &global_mpool;
}

void* gsalloc(size_t size)
{
	return mpool_salloc(&global_mpool, size);
}

void gsfree(void* ptr, size_t size)
{
	mpool_sfree(&global_mpool, ptr, size);
}


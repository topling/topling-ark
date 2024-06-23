/**
 * use 32 bit integer for object id
 * Useful in 64 bit platform, can reduce memory size
 */

#include "bpool.h"

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
#define INITIAL_CHUNKS 256

#define INVALID_ID 0xFFFFFFFF

TERARK_DLL_EXPORT int bbinit(struct ballocator* self, uint32_t cell_size)
{
	assert(cell_size >= 8);
	assert(cell_size <= 512);

	self->balloc = &bballoc;
	self->bfree = &bbfree;

	self->nUsed_cell = 0;
	self->free_slot = 0;
	self->nMaxChunks = INITIAL_CHUNKS;
	self->pChunks = (struct bchunk_entry*)malloc(sizeof(struct bchunk_entry) * INITIAL_CHUNKS);
	self->recent = self->pChunks;
	self->recent->base = NULL;
	self->recent->head = INVALID_ID;
	self->recent->used = 0;

	return NULL != self->pChunks;
}

TERARK_DLL_EXPORT void bbdestroy(struct ballocator* self)
{
	int i;
	for (i = 0; i < self->nMaxChunks; ++i) {
		if (NULL != self->pChunks[i].base)
			free(self->pChunks[i].base);
	}
	free(self->pChunks);
}

//#define GetNext(i)  self->head

TERARK_DLL_EXPORT uint32_t bballoc(struct ballocator* self)
{
	struct bchunk_entry* entry = self->recent;

	assert(NULL != entry);

	if (INVALID_ID != entry->head) {
		uint32_t h = entry->head;
		entry->head = *(uint32_t*)(entry->base + h * self->cell_size);
		entry->used++;
		self->nUsed_cell++;
		return (uint32_t)((entry - self->pChunks) * BB_CHUNK_SIZE + h);
	}
	else {
		if (self->free_slot >= self->nMaxChunks) {
			int i;
			entry = (struct bchunk_entry*)realloc(self->pChunks, sizeof(struct bchunk_entry) * self->nMaxChunks * 2);
			if (NULL == entry)
				return INVALID_ID;
			for (i = self->nChunks; i < 2*self->nChunks; ++i) {
				entry[i].base = NULL;
				entry[i].head = INVALID_ID;
				entry[i].used = 0;
			}
			self->free_slot = self->nMaxChunks;
			self->nMaxChunks *= 2;
			self->pChunks = entry;
		}
		/** last entry */
		entry = self->pChunks + self->free_slot;
		entry->base = (char*)malloc(self->cell_size * BB_CHUNK_SIZE);
		if (NULL == entry->base)
			return INVALID_ID;
		{ // this block may take a long time
			int i;
			for (i = 0; i < BB_CHUNK_SIZE-1; ++i) {
				*(uint32_t*)(entry->base + self->cell_size * i) = i + 1;
			}
			*(uint32_t*)(entry->base + self->cell_size * i) = INVALID_ID;
		}
		entry->used = 1; // first allocated
		self->nUsed_cell++;
		self->recent = entry;
		self->free_slot++;
		return (uint32_t)((entry - self->pChunks) * BB_CHUNK_SIZE);
	}
}

TERARK_DLL_EXPORT void bbfree(struct ballocator* self, uint32_t id)
{
	int iChunk = id / BB_CHUNK_SIZE;
	struct bchunk_entry* entry = self->pChunks + iChunk;

	assert(iChunk < self->nMaxChunks);
	assert(iChunk != self->free_slot);

	if (1 == entry->used) {
		free(entry->base);
		entry->base = NULL;
		entry->head = INVALID_ID;
		entry->used = 0;
		if (self->free_slot > iChunk) {
			/** this makes the allocated id likely small
			 */
			self->free_slot = iChunk;
		}
	}
	else {
		if (entry->used > self->recent->used) {
			/** self->recent prefer the entry which used more
			 *    this makes the more used chunks likely be allocated
			 *    and less used chunks likely be freed
			 */
			self->recent = entry;
		}
		entry->used++;
		*(uint32_t*)(entry->base + (id % BB_CHUNK_SIZE) * self->cell_size) = entry->head;
	}
	self->nUsed_cell--;
}

TERARK_DLL_EXPORT void* bbaddress(struct ballocator* self, uint32_t id)
{
	int iChunk = id / BB_CHUNK_SIZE;
	int iCell  = id % BB_CHUNK_SIZE;
	struct bchunk_entry* entry = self->pChunks + iChunk;

	assert(iChunk < self->nMaxChunks);
	assert(NULL != entry->base);
	assert(0 != entry->used);

	return entry->base + self->cell_size * iCell;
}


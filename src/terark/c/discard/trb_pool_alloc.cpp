#include <assert.h>
#include "trb.h"

#ifdef __GNUC__
// #  include <ext/pool_allocator.h>
// #  define trb_pool_allocator_base __gnu_cxx::__pool_alloc
#  include <ext/mt_allocator.h>
#  define trb_pool_allocator_base __gnu_cxx::__mt_alloc
#else
#  include <vector>
#  define trb_pool_allocator_base std::allocator
#endif

#if defined(_DEBUG) || !defined(NDEBUG)
#  include <atomic>

	struct trb_pool_allocator : public trb_pool_allocator_base<unsigned char>
	{
		std::atomic<intptr_t> cnt;

		trb_pool_allocator() : cnt(0)
		{
		}

		~trb_pool_allocator()
		{
			assert(0 == cnt);
		}

		void* alloc(size_t size)
		{
			// only 1 unsigned is used, but reserve an extra unsigned, for better align
			size_t extra = sizeof(unsigned)*2;
			++cnt;
			unsigned* p = (unsigned*)this->allocate(size + extra);
			p[0] = size;
			return p+2;
		}

		void free(void* block, size_t size)
		{
			size_t extra = sizeof(unsigned)*2;
			--cnt;
			unsigned* p = (unsigned*)block;
			assert(p[-2] == size);
			this->deallocate((unsigned char*)(p-2), size + extra);
		}
	};

#else // _DEBUG

	struct trb_pool_allocator : public trb_pool_allocator_base<unsigned char>
	{
		void* alloc(size_t size)
		{
			return this->allocate(size);
		}

		void free(void* block, size_t size)
		{
			this->deallocate((unsigned char*)block, size);
		}
	};

#endif // _DEBUG

static trb_pool_allocator G_trb_pool_alloc;

extern "C" {

void*
trb_pool_alloc(const struct trb_vtab* vtab,
			   struct trb_tree* tree,
			   size_t size)
{
	return G_trb_pool_alloc.alloc(size);
}

void
trb_pool_free(const struct trb_vtab* vtab,
				struct trb_tree* tree,
				void* block,
				size_t size)
{
	G_trb_pool_alloc.free((unsigned char*)block, size);
}

};

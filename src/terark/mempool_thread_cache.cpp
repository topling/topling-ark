// #pragma GCC optimize("O0")
// #pragma GCC optimize("no-inline")
// #pragma GCC optimize("no-omit-frame-pointer")
#include "mempool_thread_cache.hpp"
#include <terark/util/atomic.hpp>
#include <stdexcept>
#include <boost/integer/static_log2.hpp>
#include <boost/mpl/if.hpp>
#include <mutex>
#if defined(_MSC_VER)
#   define NOMINMAX
#   define WIN32_LEAN_AND_MEAN
#	include <io.h>
#   include <windows.h>
#else
#include <sys/mman.h>
#endif
#include <terark/util/hugepage.hpp>
#include <terark/util/profiling.hpp> // for qtime

#if defined(__SANITIZE_ADDRESS__)
  #include <sanitizer/asan_interface.h>
#else
  #define ASAN_POISON_MEMORY_REGION(addr,size)   ((void)(addr), (void)(size))
  #define ASAN_UNPOISON_MEMORY_REGION(addr,size) ((void)(addr), (void)(size))
#endif

#if defined(__SANITIZE_MEMORY__)
  #include <sanitizer/msan_interface.h>
  #define MSAN_ALLOCATED_MEMORY       __msan_allocated_memory
  #define MSAN_UNPOISON_MEMORY_REGION __msan_unpoison
#else
  #define MSAN_ALLOCATED_MEMORY(addr,size)       ((void)(addr), (void)(size))
  #define MSAN_UNPOISON_MEMORY_REGION(addr,size) ((void)(addr), (void)(size))
#endif
#define MSAN_POISON_MEMORY_REGION MSAN_ALLOCATED_MEMORY

//#define TERARK_MPTC_USE_SKIPLIST

namespace terark {

#define TCMemPoolOneThreadMF(...) template<int AlignSize> __VA_ARGS__ TCMemPoolOneThread<AlignSize>::
#define ThreadCacheMemPoolMF(...) template<int AlignSize> __VA_ARGS__ ThreadCacheMemPool<AlignSize>::

TCMemPoolOneThreadMF()TCMemPoolOneThread(ThreadCacheMemPool<AlignSize>* mp) {
    m_mempool = mp;
    m_next_free = nullptr;
    m_freelist_head.resize(mp->m_fastbin_max_size / AlignSize);
    fragment_size = 0;
    m_frag_inc = 0;
    huge_size_sum = 0;
    huge_node_cnt = 0;
    huge_list.size = 0;
    for(auto& next : huge_list.next) next = list_tail;
    m_hot_pos = 0;
    m_hot_end = 0;
}
TCMemPoolOneThreadMF()~TCMemPoolOneThread() {
}
TCMemPoolOneThreadMF(size_t)random_level() {
    size_t level = 1;
    while (rand() % 4 == 0 && level < skip_list_level_max)
        ++level;
    return level - 1;
}

#if defined(NDEBUG) || defined(__SANITIZE_ADDRESS__)
    #define mptc1t_debug_fill_alloc(mem, len)
    #define mptc1t_debug_fill_free(mem, len)
#else
    static void mptc1t_debug_fill_alloc(void* mem, size_t len) {
        memset(mem, 0xCC, len);
        MSAN_POISON_MEMORY_REGION(mem, len); // can't read before write
    }
    static void mptc1t_debug_fill_free(void* mem, size_t len) {
        memset(mem, 0xDD, len);
        MSAN_POISON_MEMORY_REGION(mem, len); // can't read before write
    }
#endif

TCMemPoolOneThreadMF(void)reduce_frag_size(size_t request) {
    fragment_size -= request;
    m_frag_inc -= request;
    if (m_frag_inc < -256 * 1024) {
        as_atomic(m_mempool->fragment_size).
            fetch_sub(size_t(-m_frag_inc), std::memory_order_relaxed);
        m_frag_inc = 0;
    }
}

TCMemPoolOneThreadMF(terark_no_inline size_t)
alloc(byte_t* base, size_t request) {
    assert(request % AlignSize == 0);
    size_t loop_cnt = 0;
    #define MPTC_CHECK_loop_cnt() do { \
        if (loop_cnt > 200) { \
            fprintf(stderr, "%s:%d: loop_cnt = %zd\n", __FILE__, __LINE__, loop_cnt); \
        }} while (0)
    if (terark_likely(request <= m_freelist_head.size() * AlignSize)) {
        size_t idx = request / AlignSize - 1;
        auto& list = m_freelist_head[idx];
        size_t next = list.head;
        if (list_tail != next) {
            size_t pos = size_t(next) * AlignSize;
            reduce_frag_size(request);
            ASAN_UNPOISON_MEMORY_REGION(base + pos, request);
            list.cnt--;
            list.head = *(link_size_t*)(base + pos);
            mptc1t_debug_fill_alloc(base + pos, request);
            return pos;
        }
        else {
            // try 2x size in freelist
            size_t idx2 = idx * 2 + 1;
            if (idx2 < m_freelist_head.size()) {
                auto& list2 = m_freelist_head[idx2];
                next = list2.head;
                if (list_tail != next) {
                    size_t pos = size_t(next) * AlignSize;
                    reduce_frag_size(request);
                    ASAN_UNPOISON_MEMORY_REGION(base + pos, 2*request);
                    list2.cnt--;
                    list2.head = *(link_size_t*)(base + pos);
                    // put remain half to 'list'...
                    // list.head must be list_tail
                //  *(link_size_t*)(base + pos + request) = list.head;
                    *(link_size_t*)(base + pos + request) = list_tail;
                    list.cnt++;
                    list.head = (pos + request) / AlignSize;
                    ASAN_POISON_MEMORY_REGION(base + pos + request, request);
                    mptc1t_debug_fill_alloc(base + pos, request);
                    return pos;
                }
            }
        }
        {
            assert(m_hot_pos <= m_hot_end);
            assert(m_hot_end <= m_mempool->size());
            size_t pos = m_hot_pos;
            size_t End = pos + request;
            if (terark_likely(End <= m_hot_end)) {
                m_hot_pos = End;
                ASAN_UNPOISON_MEMORY_REGION(base + pos, request);
                mptc1t_debug_fill_alloc(base + pos, request);
                return pos;
            }
        }
      #if defined(TERARK_MPTC_USE_SKIPLIST)
        // set huge_list largest block as {m_hot_pos,m_hot_end} if exists
        huge_link_t* update[skip_list_level_max];
        huge_link_t* n2 = &huge_list;
        if (huge_list.size) {
            size_t k = huge_list.size - 1;
            huge_link_t* n1 = nullptr;
            while (true) {
                loop_cnt++;
                while (n2->next[k] != list_tail) {
                  loop_cnt++;
                  n1 = n2;
                  n2 = (huge_link_t*)(base + (size_t(n2->next[k]) << offset_shift));
                }
                update[k] = n1;
                if (k-- > 0)
                    n2 = n1;
                else
                    break;
            }
        }
        if (n2->size >= request) {
            size_t rlen = n2->size;
            size_t res = size_t((byte*)n2 - base);
            size_t res_shift = res >> offset_shift;
            for (size_t k = 0; k < huge_list.size; ++k) {
                loop_cnt++;
                if (update[k]->next[k] == res_shift)
                    update[k]->next[k] = n2->next[k];
            }
            while (huge_list.next[huge_list.size - 1] == list_tail && --huge_list.size > 0)
                loop_cnt++;
            if (m_hot_pos < m_hot_end) {
                sfree(base, m_hot_pos, m_hot_end - m_hot_pos);
            }
            m_hot_pos = res + request;
            m_hot_end = res + rlen;
            huge_size_sum -= rlen;
            huge_node_cnt -= 1;
            reduce_frag_size(rlen);
            ASAN_UNPOISON_MEMORY_REGION(base + res, request);
            mptc1t_debug_fill_alloc(base + res, request);
            MPTC_CHECK_loop_cnt();
            return res;
        }
      #else
        if (list_tail != size_t(huge_list.next[0])) {
            size_t res = size_t(huge_list.next[0]) << offset_shift;
            size_t rlen = ((huge_link_t*)(base + res))->size;
            if (rlen >= request) {
                huge_list.next[0] = ((huge_link_t*)(base + res))->next[0];
                if (m_hot_pos < m_hot_end) {
                    sfree(base, m_hot_pos, m_hot_end - m_hot_pos);
                }
                m_hot_pos = res + request;
                m_hot_end = res + rlen;
                huge_size_sum -= rlen;
                huge_node_cnt -= 1;
                reduce_frag_size(rlen);
                ASAN_UNPOISON_MEMORY_REGION(base + res, request);
                mptc1t_debug_fill_alloc(base + res, request);
                return res;
            }
        }
      #endif
    }
    else {
        assert(request >= sizeof(huge_link_t));
      #if defined(TERARK_MPTC_USE_SKIPLIST)
        huge_link_t* update[skip_list_level_max];
        huge_link_t* n1 = &huge_list;
        huge_link_t* n2 = nullptr;
        for (size_t k = huge_list.size; k > 0; ) {
            loop_cnt++;
            k--;
            while (n1->next[k] != list_tail && (n2 = ((huge_link_t*)(base + (size_t(n1->next[k]) << offset_shift))))->size < request)
                loop_cnt++,
                n1 = n2;
            update[k] = n1;
        }
        if (n2 != nullptr && n2->size >= request) {
            assert((byte*)n2 >= base);
            size_t n2_size = n2->size;
            size_t remain = n2_size - request;
            size_t res = size_t((byte*)n2 - base);
            size_t res_shift = res >> offset_shift;
            for (size_t k = 0; k < huge_list.size; ++k)
                if ((n1 = update[k])->next[k] == res_shift)
                    loop_cnt++,
                    n1->next[k] = n2->next[k];
            while (huge_list.next[huge_list.size - 1] == list_tail && --huge_list.size > 0)
                loop_cnt++;
            if (remain)
                sfree(base, res + request, remain);
            huge_size_sum -= n2_size; // n2 is deleted from hugelist
            huge_node_cnt -= 1;
            reduce_frag_size(n2_size);
            ASAN_UNPOISON_MEMORY_REGION(base + res, request);
            mptc1t_debug_fill_alloc(base + res, request);
            MPTC_CHECK_loop_cnt();
            return res;
        }
      #else
        if (list_tail != size_t(huge_list.next[0])) {
            size_t res = size_t(huge_list.next[0]) << offset_shift;
            size_t rlen = ((huge_link_t*)(base + res))->size;
            if (rlen >= request) {
                huge_list.next[0] = ((huge_link_t*)(base + res))->next[0];
                huge_size_sum -= rlen;
                huge_node_cnt -= 1;
                reduce_frag_size(rlen);
                if (rlen > request) {
                    sfree(base, res + request, rlen - request);
                }
                ASAN_UNPOISON_MEMORY_REGION(base + res, request);
                mptc1t_debug_fill_alloc(base + res, request);
                return res;
            }
        }
      #endif
        assert(m_hot_pos <= m_hot_end);
        assert(m_hot_end <= m_mempool->size());
        size_t pos = m_hot_pos;
        size_t End = pos + request;
        if (terark_likely(End <= m_hot_end)) {
            m_hot_pos = End;
            ASAN_UNPOISON_MEMORY_REGION(base + pos, request);
            mptc1t_debug_fill_alloc(base + pos, request);
            MPTC_CHECK_loop_cnt();
            return pos;
        }
    }
    MPTC_CHECK_loop_cnt();
    return size_t(-1); // fail
}


TCMemPoolOneThreadMF(size_t)
alloc3(byte_t* base, size_t oldpos, size_t oldlen, size_t newlen) {
    assert(oldpos % AlignSize == 0);
    assert(oldlen % AlignSize == 0);
    assert(newlen % AlignSize == 0);
    if (oldpos + oldlen == m_hot_pos) {
        size_t newend = oldpos + newlen;
        if (newend <= m_hot_end) {
            m_hot_pos = newend;
        #if defined(__SANITIZE_ADDRESS__)
            if (newlen > oldlen)
              ASAN_UNPOISON_MEMORY_REGION(base + oldpos, newlen);
            else if (newlen < oldlen)
              ASAN_POISON_MEMORY_REGION(base + newend, oldlen - newlen);
        #endif
            return oldpos;
        }
    }
    if (newlen < oldlen) {
        assert(oldlen - newlen >= sizeof(link_t));
        assert(oldlen - newlen >= AlignSize);
        sfree(base, oldpos + newlen, oldlen - newlen);
        return oldpos;
    }
    else if (newlen == oldlen) {
        // do nothing
        return oldpos;
    }
    else {
        size_t newpos = alloc(base, newlen);
        if (size_t(-1) != newpos) {
            memcpy(base + newpos, base + oldpos, std::min(oldlen, newlen));
            sfree(base, oldpos, oldlen);
        }
        return newpos;
    }
}

TCMemPoolOneThreadMF(void)sfree(byte_t* base, size_t pos, size_t len) {
    assert(pos % AlignSize == 0);
    assert(len % AlignSize == 0);
    assert(len >= sizeof(link_t));
    if (terark_unlikely(pos + len == m_hot_pos)) {
        ASAN_POISON_MEMORY_REGION(base + pos, len);
        m_hot_pos = pos;
        return;
    }
    if (terark_likely(len <= m_freelist_head.size() * AlignSize)) {
        size_t idx = len / AlignSize - 1;
        auto& list = m_freelist_head[idx];
        mptc1t_debug_fill_free((link_t*)(base + pos) + 1, len-sizeof(link_t));
        *(link_size_t*)(base + pos) = list.head;
        ASAN_POISON_MEMORY_REGION(base + pos, len);
        list.head = link_size_t(pos / AlignSize);
        list.cnt++;
    }
    else {
        size_t loop_cnt = 0;
        assert(len >= sizeof(huge_link_t));
      #if defined(TERARK_MPTC_USE_SKIPLIST)
        huge_link_t* update[skip_list_level_max];
        huge_link_t* n1 = &huge_list;
        huge_link_t* n2;
        size_t rand_lev = random_level();
        size_t k = huge_list.size;
        while (k-- > 0) {
            loop_cnt++;
            while (n1->next[k] != list_tail && (n2 = ((huge_link_t*)(base + (size_t(n1->next[k]) << offset_shift))))->size < len)
                loop_cnt++,
                n1 = n2;
            update[k] = n1;
        }
        if (rand_lev >= huge_list.size) {
            k = huge_list.size++;
            update[k] = &huge_list;
        }
        else {
            k = rand_lev;
        }
        n2 = (huge_link_t*)(base + pos);
        size_t pos_shift = pos >> offset_shift;
        do {
            loop_cnt++;
            n1 = update[k];
            n2->next[k] = n1->next[k];
            n1->next[k] = pos_shift;
        } while(k-- > 0);
        n2->size = len;
      #else
        huge_link_t* n2 = (huge_link_t*)(base + pos);
        n2->size = link_size_t(len);
        n2->next[0] = huge_list.next[0];
        huge_list.next[0] = link_size_t(pos >> offset_shift);
      #endif
        mptc1t_debug_fill_free(n2 + 1, len - sizeof(*n2));
        ASAN_POISON_MEMORY_REGION(n2 + 1, len - sizeof(*n2));
        huge_size_sum += len;
        huge_node_cnt += 1;
        MPTC_CHECK_loop_cnt();
    }
    fragment_size += len;
    m_frag_inc += len;
    if (m_frag_inc > 256 * 1024) {
        as_atomic(m_mempool->fragment_size).
            fetch_add(size_t(m_frag_inc), std::memory_order_relaxed);
        m_frag_inc = 0;
    }
}

TCMemPoolOneThreadMF(void)
set_hot_area(byte_t* base, size_t pos, size_t len) {
    if (m_hot_end == pos) {
        // do not need to change m_hot_pos
        m_hot_end = pos + len;
    }
    else {
        if (m_hot_pos < m_hot_end) {
            size_t large_len = m_hot_end - m_hot_pos;
            ASAN_UNPOISON_MEMORY_REGION(base + m_hot_pos, large_len);
            sfree(base, m_hot_pos, large_len);
        }
        else {
            // do nothing
            TERARK_IF_MSVC(TERARK_IF_DEBUG(int debug1 = 0,),);
        }
        m_hot_pos = pos;
        m_hot_end = pos + len;
    }
}

TCMemPoolOneThreadMF(void)
populate_hot_area(byte_t* base, size_t pageSize) {
    for (size_t pos = m_hot_pos; pos < m_hot_end; pos += pageSize) {
        base[pos] = 0;
    }
}

// called on current thread exit
TCMemPoolOneThreadMF(void)clean_for_reuse() {}

// called after a previous thread call clean_for_reuse
TCMemPoolOneThreadMF(void)init_for_reuse() {}

///////////////////////////////////////////////////////////////////////////////

ThreadCacheMemPoolMF(void)clean_for_reuse(TCMemPoolOneThread<AlignSize>* t) {
    t->clean_for_reuse();
    as_atomic(fragment_size).fetch_add(t->m_frag_inc, std::memory_order_relaxed);
    t->m_frag_inc = 0;
}

ThreadCacheMemPoolMF(void)init_for_reuse(TCMemPoolOneThread<AlignSize>* t) const {
    t->init_for_reuse();
}

ThreadCacheMemPoolMF()ThreadCacheMemPool(size_t fastbin_max_size) {
    assert(fastbin_max_size >= AlignSize);
    assert(fastbin_max_size >= sizeof(typename TCMemPoolOneThread<AlignSize>::huge_link_t));
    fragment_size = 0;
    m_fastbin_max_size = pow2_align_up(fastbin_max_size, AlignSize);
    m_new_tc = &default_new_tc;
}

ThreadCacheMemPoolMF()~ThreadCacheMemPool() {
    // do nothing
}

ThreadCacheMemPoolMF(void)sync_frag_size() {
    this->for_each_tls([this](TCMemPoolOneThread<AlignSize>* tc) {
        this->fragment_size += tc->m_frag_inc;
        tc->m_frag_inc = 0;
    });
}

// when calling this function, there should not be any other concurrent
// thread accessing this mempool's meta data.
// after this function call, this->fragment_size includs hot area free size
ThreadCacheMemPoolMF(void)sync_frag_size_full() {
    this->fragment_size = 0;
    this->for_each_tls([this](TCMemPoolOneThread<AlignSize>* tc) {
        size_t hot_len = tc->m_hot_end - tc->m_hot_pos;
        this->fragment_size += tc->fragment_size + hot_len;
    });
}


// the frag_size including hot area of each thread cache and
// fragments in freelists
ThreadCacheMemPoolMF(size_t)slow_get_free_size() const {
    size_t sz = 0;
    this->for_each_tls([&sz](TCMemPoolOneThread<AlignSize>* tc) {
        size_t hot_end, hot_pos;
        do {
            hot_end = tc->m_hot_end;
            hot_pos = tc->m_hot_pos;
            // other threads may updating hot_pos and hot_end which
            // cause race condition and make hot_pos > hot_end
        } while (terark_unlikely(hot_pos > hot_end));
        sz += hot_end - hot_pos;
        sz += tc->fragment_size;
    });
    return sz;
}

ThreadCacheMemPoolMF(size_t)get_cur_tls_free_size() const {
    auto tc = this->get_tls_or_null();
    if (nullptr == tc) {
        return 0;
    }
    size_t hot_len = tc->m_hot_end - tc->m_hot_pos;
    return tc->fragment_size + hot_len;
}

ThreadCacheMemPoolMF(void)destroy_and_clean() {
    mem::clear();
}

ThreadCacheMemPoolMF(void)get_fastbin(valvec<size_t>* fast) const {
    fast->resize_fill(m_fastbin_max_size/AlignSize, 0);
    this->for_each_tls([fast](TCMemPoolOneThread<AlignSize>* tc) {
        auto _p = tc->m_freelist_head.data();
        auto _n = tc->m_freelist_head.size();
        size_t* fastptr = fast->data();
        for (size_t i = 0; i < _n; ++i) {
            fastptr[i] += _p[i].cnt;
        }
    });
}

ThreadCacheMemPoolMF(void)print_stat(FILE* fp) const {
    size_t ti = 0, computed_frag_size = 0, computed_hot_size = 0;
    fprintf(fp, "threads=%zd, frag=%zd\n", this->m_tls_vec.size(), fragment_size);
    this->for_each_tls([&](TCMemPoolOneThread<AlignSize>* tc) {
        auto _p = tc->m_freelist_head.data();
        auto _n = tc->m_freelist_head.size();
        size_t frag_num = tc->huge_node_cnt;
        for (size_t i = 0; i < _n; ++i) {
            frag_num += _p[i].cnt;
        }
        size_t hotlen = tc->m_hot_end - tc->m_hot_pos;
        fprintf(fp, "  thread %zd: frag{num=%zd,len=%zd,inc=%zd}, huge{num=%zd,len=%zd}, hotlen=%zd\n    fastbin: ",
            ti, frag_num, tc->fragment_size, tc->m_frag_inc, tc->huge_node_cnt, tc->huge_size_sum, hotlen);
        size_t len = 0;
        for (size_t i = 0; i < _n; ++i) {
            if (_p[i].cnt) {
                fprintf(fp, "(%zd, %zd), ", i, size_t(_p[i].cnt));
                len += AlignSize * (i + 1) * _p[i].cnt;
                computed_frag_size += len;
            }
        }
        computed_frag_size += tc->huge_size_sum;
        computed_hot_size += hotlen;
        fprintf(fp, "len = %zd\n", len);
        ti++;
    });
    fprintf(fp, "computed_frag_size = %zd, computed_hot_size = %zd, plus the two = %zd\n",
                    computed_frag_size, computed_hot_size, computed_frag_size + computed_hot_size);
}

ThreadCacheMemPoolMF(size_t)get_huge_stat(size_t* huge_memsize) const {
    size_t huge_size_sum = 0;
    size_t huge_node_cnt = 0;
    this->for_each_tls([&](TCMemPoolOneThread<AlignSize>* tc) {
        huge_size_sum += tc->huge_size_sum;
        huge_node_cnt += tc->huge_node_cnt;
    });
    *huge_memsize = huge_size_sum;
    return huge_node_cnt;
}

ThreadCacheMemPoolMF(void)risk_set_data(const void* data, size_t len) {
    assert(NULL == mem::p);
    assert(0 == mem::n);
    assert(0 == mem::c);
    mem::risk_set_data((unsigned char*)data, len);
}

ThreadCacheMemPoolMF(void)clear() {
}

ThreadCacheMemPoolMF(void)erase_all() {
}


ThreadCacheMemPoolMF(terark_no_inline void)reserve(size_t cap) {
    cap = pow2_align_up(cap, ArenaSize);
    size_t oldsize = mem::n;
    use_hugepage_resize_no_init(this, cap);
    mem::n = oldsize;
    ASAN_POISON_MEMORY_REGION(mem::p + oldsize, mem::c - oldsize);
    MSAN_POISON_MEMORY_REGION(mem::p + oldsize, mem::c - oldsize);
}

ThreadCacheMemPoolMF(void)shrink_to_fit() {}


    // should not throw
ThreadCacheMemPoolMF(terark_no_inline bool)
chunk_alloc(TCMemPoolOneThread<AlignSize>* tc, size_t request) {
    size_t  chunk_len; // = pow2_align_up(request, m_chunk_size);
    size_t  cap  = mem::c;
    size_t  oldn; // = mem::n;
    byte_t* base = mem::p;
    do {
        chunk_len = pow2_align_up(request, m_chunk_size);
        oldn = mem::n;
        size_t endpos = size_t(base + oldn);
        if (terark_unlikely((endpos & (m_chunk_size-1)) != 0)) {
            chunk_len += m_chunk_size - (endpos & (m_chunk_size-1));
        }
        if (terark_unlikely(oldn + chunk_len > cap)) {
            if (oldn + request > cap) {
                // cap is fixed, so fail
                return false;
            }
            chunk_len = cap - oldn;
        }
        assert(oldn + chunk_len <= cap);
    } while (!cas_weak(mem::n, oldn, oldn + chunk_len));

  #if defined(_MSC_VER)
    // Windows requires explicit commit virtual memory
    size_t beg = pow2_align_down(size_t(base + oldn), 4096);
    size_t end = pow2_align_up(size_t(base + oldn + chunk_len), 4096);
    size_t len = end - beg;
    if (!VirtualAlloc((void*)beg, end, MEM_COMMIT, PAGE_READWRITE)) {
        double numMiB = double(len) / (1<<20);
        TERARK_DIE("VirtualAlloc(ptr=%zX, len=%fMiB, COMMIT) = %d",
                   beg, numMiB, GetLastError());
    }
  #elif defined(__linux__)
    // linux is implicit commit virtual memory, if memory is insufficient,
    // SIGFAULT/SIGBUS will be generated. We allow users explicit commit
    // virtual memory by madvise(POPULATE_WRITE), POPULATE_WRITE is a new
    // feature since kernel version 5.14
    if (m_vm_explicit_commit) {
        auto t0 = qtime::now();
        // TERARK_VERIFY_AL(size_t(base), ArenaSize); // not needed and may fail
        size_t beg = pow2_align_down(size_t(base + oldn), m_chunk_size);
        size_t end = pow2_align_up(size_t(base + oldn + chunk_len), m_chunk_size);
        size_t len = end - beg;
        const int POPULATE_WRITE = 23; // older kernel has no MADV_POPULATE_WRITE
        while (madvise((void*)beg, len, POPULATE_WRITE) != 0) {
            int err = errno;
            if (EFAULT == err) {
                TERARK_DIE("MADV_POPULATE_WRITE(%zd) = EFAULT: is vm.nr_hugepages insufficient?", len);
                break;
            }
            else if (EAGAIN == err) {
                continue; // try again
            }
            else if (EINVAL == err) {
                break; // old kernel, or other errors, ignore
            }
            else if (true) {
                as_atomic(m_vm_commit_fail_cnt).fetch_add(1, std::memory_order_relaxed);
                as_atomic(m_vm_commit_fail_len).fetch_add(len, std::memory_order_relaxed);
                break;
            }
            else {
                double numMiB = double(len) / (1<<20);
                TERARK_DIE("madvise(ptr=%zX, len=%fMiB, POPULATE_WRITE) = %m", beg, numMiB);
            }
        }
        auto t1 = qtime::now();
        if (t1.us(t0) > 100) {
            extern const char* StrDateTimeNow(); // defined in nolocks_localtime.cpp
            fprintf(stderr, "%s: WARN: %s: POPULATE_WRITE(%zd) = %.3f ms\n",
                    StrDateTimeNow(), "ThreadCacheMemPool::chunk_alloc", len, t1.mf(t0));
        }
    }
  #endif

    tc->set_hot_area(base, oldn, chunk_len);
    return true;
}

ThreadCacheMemPoolMF(TCMemPoolOneThread<AlignSize>*)
default_new_tc(ThreadCacheMemPool* mp) {
    return new TCMemPoolOneThread<AlignSize>(mp);
}

// param request must be aligned by AlignSize
ThreadCacheMemPoolMF(size_t)alloc(size_t request) {
    assert(request > 0);
    auto tc = this->get_tls();
    if (terark_unlikely(nullptr == tc)) {
        return size_t(-1); // fail
    }
    return alloc(request, tc);
}

ThreadCacheMemPoolMF(size_t)
alloc3(size_t oldpos, size_t oldlen, size_t newlen) {
    assert(newlen > 0);
    assert(oldlen > 0);
    auto tc = this->get_tls();
    return alloc3(oldpos, oldlen, newlen, tc);
}

ThreadCacheMemPoolMF(size_t)
alloc3(size_t oldpos, size_t oldlen, size_t newlen,
       TCMemPoolOneThread<AlignSize>* tc)
{
    assert(newlen > 0); newlen = pow2_align_up(newlen, AlignSize);
    assert(oldlen > 0); oldlen = pow2_align_up(oldlen, AlignSize);
    if (AlignSize < sizeof(link_t)) { // const expression
        newlen = std::max(sizeof(link_t), newlen);
    }
    size_t res = tc->alloc3(mem::p, oldpos, oldlen, newlen);
    if (terark_unlikely(size_t(-1) == res)) {
        assert(oldlen < newlen);
        if (terark_likely(chunk_alloc(tc, newlen))) {
            res = tc->alloc(mem::p, newlen);
            if (terark_likely(size_t(-1) != res)) {
                memcpy(mem::p + res, mem::p + oldpos, oldlen);
                tc->sfree(mem::p, oldpos, oldlen);
            }
        }
    }
    return res;
}

ThreadCacheMemPoolMF(size_t)
alloc_slow_path(size_t request, TCMemPoolOneThread<AlignSize>* tc) {
    if (chunk_alloc(tc, request))
        return tc->alloc(mem::p, request);
    else
        return size_t(-1);
}

ThreadCacheMemPoolMF(void)sfree(size_t pos, size_t len) {
    assert(len > 0);
    assert(pos < mem::n);
    assert(pos % AlignSize == 0);
    auto tc = this->get_tls();
    sfree(pos, len, tc);
}

ThreadCacheMemPoolMF(void)tc_populate(size_t sz) {
    size_t  chunk_len; // = pow2_align_down(sz, m_chunk_size);
    size_t  cap  = mem::c;
    size_t  oldn; // = mem::n;
    byte_t* base = mem::p;
    do {
        chunk_len = pow2_align_down(sz, m_chunk_size);
        oldn = mem::n;
        size_t endpos = size_t(base + oldn);
        if (terark_unlikely(endpos % m_chunk_size != 0)) {
            chunk_len += m_chunk_size - (endpos & (m_chunk_size-1));
        }
        if (terark_unlikely(oldn + chunk_len > cap)) {
            chunk_len = cap - oldn;
        }
        assert(oldn + chunk_len <= cap);
    } while (!cas_weak(mem::n, oldn, oldn + chunk_len));

    auto tc = this->get_tls();
    tc->set_hot_area(base, oldn, chunk_len);
    //tc->populate_hot_area(base, m_chunk_size);
    tc->populate_hot_area(base, 4*1024);
}

ThreadCacheMemPoolMF(TCMemPoolOneThread<AlignSize>*)create_tls_obj() const {
    return m_new_tc(const_cast<ThreadCacheMemPool*>(this));
}

template class TCMemPoolOneThread<4>;
template class ThreadCacheMemPool<4>;

template class TCMemPoolOneThread<8>;
template class ThreadCacheMemPool<8>;

} // namespace terark


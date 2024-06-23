
class DebugMallocLeakCheck {
    struct MallocMemInfo {
        size_t size;
        size_t malloc_seqno;
        size_t realloc_seqno;
        size_t free_seqno;
        const char* file;
        int line;
    };
    size_t m_malloc_seqno = 1;
    size_t m_realloc_seqno = 1;
    size_t m_free_seqno = 1;
    std::map<void*, MallocMemInfo> m_mem_map;
public:
    void* debug_malloc(size_t size, const char* file, int line) {
        void* mem = malloc(size);
        m_mem_map[mem] = {size, m_malloc_seqno++, m_realloc_seqno, m_free_seqno, file, line};
        return mem;
    }
    void* debug_realloc(void* old, size_t size, const char* file, int line) {
        void* mem = realloc(old, size);
        m_mem_map[mem] = {size, m_malloc_seqno, m_realloc_seqno++, m_free_seqno, file, line};
        if (mem != old) {
            if (nullptr != old) {
                TERARK_VERIFY_EQ(m_mem_map.count(old), 1);
                m_mem_map.erase(old);
            }
        }
        return mem;
    }
    void debug_free(void* mem) {
        if (mem) {
            m_free_seqno++;
            TERARK_VERIFY_EQ(m_mem_map.count(mem), 1);
            m_mem_map.erase(mem);
        }
    }
    void debug_malloc_check_memleak() {
        if (m_mem_map.empty()) {
            fprintf(stderr, "No memory leaks\n");
        } else {
            for (auto& [mem, info] : m_mem_map) {
                fprintf(stderr, "memory leak %p size %zd at %s:%d: {%zd %zd %zd}\n",
                        mem, info.size, info.file, info.line,
                        info.malloc_seqno, info.realloc_seqno, info.free_seqno);
            }
            TERARK_DIE("%zd blocks memory leaks", m_mem_map.size());
        }
    }
    ~DebugMallocLeakCheck() {
        debug_malloc_check_memleak();
    }
};

DebugMallocLeakCheck g_leak_check;

#define malloc(size)       g_leak_check.debug_malloc (size, __FILE__, __LINE__)
#define realloc(old, size) g_leak_check.debug_realloc(old, size, __FILE__, __LINE__)
#define free               g_leak_check.debug_free

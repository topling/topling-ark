#pragma once
#include <new>
#include <memory>
#include <mutex>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <boost/noncopyable.hpp>
#include <terark/valvec.hpp>
#include <terark/util/atomic.hpp>

#if defined(__GNUC__) && !defined(TOPLING_USE_DYNAMIC_TLS)
  #define TERARK_RAW_TLS __thread
#elif defined(_MSC_VER)
  #define TERARK_RAW_TLS __declspec(thread)
#else
  #define TERARK_RAW_TLS thread_local
#endif

namespace terark {

template<class T>
T* tls_born(size_t n) {
    T* p = (T*)malloc(sizeof(T) * n);
    for (size_t i = 0; i < n; ++i)
        new (p+i) T ();
    return p;
}
template<class T>
void tls_die(T* p, size_t n) {
    if (!boost::has_trivial_destructor<T>::value)
        for (size_t i = 0; i < n; ++i)
            p[i].~T();
    free(p);
}
template<class T>
void tls_reborn(T* p) {
    if (!boost::has_trivial_destructor<T>::value)
        p->~T();
    new(p) T(); // call default cons
}

/// @param T default T::T() should initilize T into <null> state
///          default T::T() should not throw
template<class T, uint32_t Rows = 256, uint32_t Cols = Rows>
class instance_tls : boost::noncopyable {
    static_assert(Cols && (Cols & (Cols-1)) == 0, "Cols must be power of 2");
    // All allocated slots are initially constructed by default cons
    // 1.     busy slot is ready to be accessed
    // 2.     busy slot must be constructed(default cons to <null> state)
    // 3.     busy slot may also be really initialized(not <null>)
    // 4. non-busy slot must be <null>(default cons'ed), and will not be
    //    used by user code
    // 5. destructor will always be called on all slots
    static const uint32_t dead = uint32_t(-3); // for debug track
    static const uint32_t busy = uint32_t(-2); // for debug track
    static const uint32_t tail = uint32_t(-1);
public:
    instance_tls() {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (tail != s_id_head) {
            m_id = s_id_head;
            s_id_head = s_id_list[s_id_head];
            s_id_list[m_id] = uint32_t(busy);
            s_id_num_free--;
        }
        else if (s_id_list.size() < Rows * Cols) {
            m_id = (uint32_t)s_id_list.size();
            s_id_list.push_back(uint32_t(busy));
        }
        else { // fail
            valvec<char> buf(s_id_list.size() * 10, valvec_reserve());
            for (auto id : s_id_list) {
                int len = sprintf(buf.ensure_unused(32), "%d,", id);
                buf.risk_set_size(buf.size() + len);
            }
            buf.back() = '\0'; // replace last ',' to '\0'
            TERARK_DIE("too many instances, max = %u : {%s}", Rows*Cols, buf.data());
        }
    }
    ~instance_tls() {
        TERARK_VERIFY_LT(m_id, s_id_list.size());
        size_t id = as_atomic(m_id).exchange(dead, std::memory_order_relaxed);
        size_t i = id / Cols;
        size_t j = id % Cols;
      s_mutex.lock();
        for(MatrixLink* p = s_matrix_head.next; p != &s_matrix_head; ) {
            Matrix* q = static_cast<Matrix*>(p);
            if (q->A[i]) {
                tls_reborn(&q->A[i][j]);
            }
            p = p->next;
        }
        TERARK_VERIFY_EQ(busy, s_id_list[id]);
        s_id_list[id] = s_id_head;
        s_id_head = uint32_t(id);
        s_id_num_free++;
      s_mutex.unlock();
    }

    terark_forceinline T& get() const {
        size_t i = m_id / Cols;
        size_t j = m_id % Cols;
        Matrix* pMatrix = tls_matrix ? tls_matrix : init_tls_matrix();
        T* pOneRow = pMatrix->A[i];
        if (terark_likely(nullptr != pOneRow))
            return pOneRow[j];
        return get_slow(i, j);
    }

private:
    terark_no_inline static
    T& get_slow(size_t i, size_t j) {
        T* pOneRow = tls_born<T>(Cols);
        auto pMatrix = tls_matrix;
        TERARK_VERIFY(pMatrix != nullptr);
        // s_mutex protect for instance_tls::~instance_tls()
      s_mutex.lock();
        pMatrix->A[i] = pOneRow;
      s_mutex.unlock();
        return pOneRow[j];
    }

    uint32_t  m_id;

    // static members:
    //
    static std::mutex s_mutex;
    static uint32_t   s_thread_num;
    static uint32_t   s_id_head;
    static uint32_t   s_id_num_free;
    static valvec<uint32_t> s_id_list;

    // use matrix to speed lookup & reduce memory usage
    struct MatrixLink : boost::noncopyable {
        MatrixLink*  next;
        MatrixLink*  prev;
        MatrixLink() {} // do nothing
        MatrixLink(int) { // init as head
            next = prev = this;
        }
    };
    struct Matrix : MatrixLink {
        using MatrixLink::next;
        using MatrixLink::prev;
        T* A[Rows]; // use 2-level direct access
        void insert_to_list_in_lock() {
            next = &s_matrix_head;
            prev =  s_matrix_head.prev;
            s_matrix_head.prev->next = this;
            s_matrix_head.prev = this;
            s_thread_num++;
        }
        Matrix() {
            std::fill_n(A, Rows, nullptr);
        }
        ~Matrix() {
            size_t list_size;
          s_mutex.lock();
            next->prev = prev;
            prev->next = next;
            s_thread_num--;
            list_size = s_id_list.size();
          s_mutex.unlock();
            for (size_t i = (list_size + Cols-1) / Cols; i-- > 0; ) {
                if (T* pOneRow = A[i]) {
                    tls_die(pOneRow, Cols);
                }
            }
        }
    };
    static MatrixLink s_matrix_head;
    static TERARK_RAW_TLS TERARK_STATIC_TLS Matrix* tls_matrix; // fast
    terark_no_inline static Matrix* init_tls_matrix() {
        // thread_local is slow for complex object, so we define an simple tls
        // raw ptr and a complex object to speed up, this should done by
        // compilers, but compiler does not do it well, so we do it ourselves.
        assert(nullptr == tls_matrix);
        static thread_local Matrix tls; // init on first access
        auto pMatrix = tls_matrix = &tls;
        s_mutex.lock();
            pMatrix->insert_to_list_in_lock();
        s_mutex.unlock();
        return pMatrix;
    }
};

template<class T, uint32_t Rows, uint32_t Cols>
std::mutex
instance_tls<T, Rows, Cols>::s_mutex;

template<class T, uint32_t Rows, uint32_t Cols>
uint32_t
instance_tls<T, Rows, Cols>::s_thread_num = 0;

template<class T, uint32_t Rows, uint32_t Cols>
uint32_t
instance_tls<T, Rows, Cols>::s_id_head = tail;

template<class T, uint32_t Rows, uint32_t Cols>
valvec<uint32_t>
instance_tls<T, Rows, Cols>::s_id_list(Cols, valvec_reserve());

template<class T, uint32_t Rows, uint32_t Cols>
uint32_t
instance_tls<T, Rows, Cols>::s_id_num_free = 0;

template<class T, uint32_t Rows, uint32_t Cols>
typename
instance_tls<T, Rows, Cols>::MatrixLink
instance_tls<T, Rows, Cols>::s_matrix_head(0); // init as head

template<class T, uint32_t Rows, uint32_t Cols>
TERARK_RAW_TLS TERARK_STATIC_TLS
typename instance_tls<T, Rows, Cols>::Matrix*
instance_tls<T, Rows, Cols>::tls_matrix = nullptr;

} // namespace terark

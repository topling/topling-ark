#pragma once
// Create at 2024-03-09 16:38 by leipeng

#include "fstring.hpp"

namespace terark {

template<class T>
struct UninitializedCopyN {
  void operator()(T* dst, size_t num) const {
    if constexpr (std::is_fundamental<T>::value)
      memcpy(dst, src, sizeof(T) * num);
    else
      std::uninitialized_copy_n(src, num, dst);
  }
  const T* src;
};
template<class T>
struct UninitializedFillN {
  void operator()(T* dst, size_t num) const {
    if constexpr (std::is_fundamental<T>::value && sizeof(T) == 1)
      memset(dst, src, sizeof(T) * num);
    else
      std::uninitialized_fill_n(dst, num, src);
  }
  const T src;
};

// 1. priority of alignas is higer than #pragma pack
// 2. alignas without #pragma pack does not effect sizeof
// 3. with both the two, alignas(4) will reduce object size, thus:
//    sizeof(minimal_sso<36, 0|1, 4>) == 36
#pragma pack(push, 1)
template <size_t SizeSSO, bool WithEOS = true, size_t Align = sizeof(size_t)>
class alignas(Align) minimal_sso {
  static_assert(SizeSSO >= 32 && SizeSSO % Align == 0 && SizeSSO <= 256 - Align);
  struct Local {
    char   m_space[SizeSSO - 1];
    byte_t m_unused_len;
  };
  struct Alloc {
    char*  m_ptr;
    size_t m_len;
    size_t m_cap;
    byte_t m_pad[SizeSSO - 3 * sizeof(char*) - 1];
    byte_t m_flag;
  };
  union {
    Local m_local;
    Alloc m_alloc;
  };
  size_t local_size() const {
    TERARK_ASSERT_LE(m_local.m_unused_len, sizeof(m_local.m_space));
    return sizeof(m_local.m_space) - m_local.m_unused_len;
  }
  char* local_end() {
    TERARK_ASSERT_LE(m_local.m_unused_len, sizeof(m_local.m_space));
    return m_local.m_space + (sizeof(m_local.m_space) - m_local.m_unused_len);
  }
  char* alloc_end() {
    TERARK_ASSERT_EQ(m_local.m_unused_len, 255);
    return m_alloc.m_ptr + m_alloc.m_len;
  }
  void init_to_local_empty() {
    if (WithEOS)
      m_local.m_space[0] = '\0';
    m_local.m_unused_len = sizeof(m_local.m_space);
  }
  template<class DataPopulator>
  terark_no_inline
  void malloc_populate(size_t n, DataPopulator populate) {
    size_t cap = pow2_align_up(n + 1, 64);
    char*  ptr = (char*)malloc(cap);
    m_alloc.m_ptr = ptr;
    m_alloc.m_len = n;
    m_alloc.m_cap = cap - 1;
    m_alloc.m_flag = 255;
    populate(ptr, n);
    if (WithEOS)
      ptr[n] = '\0';
  }
  template<class DataPopulator>
  terark_no_inline
  void malloc_append(size_t addsize, DataPopulator populate,
                     size_t oldsize, size_t newsize) {
    TERARK_ASSUME(oldsize <= sizeof(m_local.m_space));
    size_t cap = pow2_align_up(newsize + 1, 64);
    char*  ptr = (char*)malloc(cap);
    memcpy(ptr, m_local.m_space, oldsize);
    populate(ptr + oldsize, addsize);
    if (WithEOS)
      ptr[newsize] = '\0';
    m_alloc.m_ptr = ptr;
    m_alloc.m_len = newsize;
    m_alloc.m_cap = cap - 1;
    m_alloc.m_flag = 255;
  }
  template<class DataPopulator>
  terark_no_inline
  void realloc_append(size_t addsize, DataPopulator populate,
                      size_t oldsize, size_t newsize) {
    size_t next_cap = m_alloc.m_cap * 103 / 64; // a bit less than 1.618
    size_t real_cap = pow2_align_up(std::max(newsize, next_cap) + 1, 64);
    m_alloc.m_ptr = (char*)realloc(m_alloc.m_ptr, real_cap);
    m_alloc.m_cap = real_cap - 1;
    populate(m_alloc.m_ptr + oldsize, addsize);
    if (WithEOS)
      m_alloc.m_ptr[newsize] = '\0';
    m_alloc.m_len = newsize;
  }

public:
  using value_type = char;
  using iterator = char*;
  using const_iterator = const char*;
  ~minimal_sso() {
    if (m_local.m_unused_len != 255) { // local
      TERARK_ASSERT_LE(m_local.m_unused_len, sizeof(m_local.m_space));
    } else {
      free(m_alloc.m_ptr);
    }
    static_assert(offsetof(minimal_sso, m_local.m_unused_len) ==
                  offsetof(minimal_sso, m_alloc.m_flag));
    static_assert(sizeof(m_local.m_space) == SizeSSO - 1);
    static_assert(sizeof(minimal_sso) == SizeSSO);
  }
  minimal_sso() { init_to_local_empty(); }
  template<class Tstring>
  minimal_sso(const Tstring& s
#if defined(_MSC_VER)
		, typename Tstring::value_type* = nullptr
		, typename Tstring::iterator* = nullptr
		, typename Tstring::const_iterator* = nullptr
#else
		, decltype(s.data()) = nullptr
#endif
  ) : minimal_sso(s.data(), s.size()) {}
  minimal_sso(const fstring  s) : minimal_sso(s.p, s.n) {}
  minimal_sso(const char* s, size_t n) : minimal_sso(n, UninitializedCopyN<char>{s}) {}
  minimal_sso(size_t n, char ch) : minimal_sso(n, UninitializedFillN<char>{ch}) {}
  template<class DataPopulator>
  minimal_sso(decltype(((*(DataPopulator*)(nullptr))((char*)0, 1), (size_t)1))
              n, // SFINAE size_t n, populate("", 1) must be well formed
              DataPopulator populate) {
    if (terark_likely(n <= sizeof(m_local.m_space))) {
      m_local.m_unused_len = sizeof(m_local.m_space) - n;
      if (WithEOS)
        m_local.m_space[n] = '\0';
      populate(m_local.m_space, n);
    } else {
      malloc_populate(n, populate);
    }
  }
  minimal_sso(size_t cap, valvec_reserve) {
    init_to_local_empty();
    reserve(cap);
  }
  minimal_sso(const minimal_sso& y) : minimal_sso(y.to<fstring>()) {}
  minimal_sso& operator=(const minimal_sso& y) {
    if (terark_likely(this != &y)) {
      this->~minimal_sso();
      new(this)minimal_sso(y);
    }
    return *this;
  }
  minimal_sso(minimal_sso&& y) {
   #if defined(TERARK_VALGRIND)
    if (y.m_local.m_unused_len != 255) { // local
      new(this)minimal_sso(y.to<fstring>());
    } else {
      m_alloc.m_ptr  = y.m_alloc.m_ptr;
      m_alloc.m_len  = y.m_alloc.m_len;
      m_alloc.m_cap  = y.m_alloc.m_cap;
      m_alloc.m_flag = 255;
    }
   #else
    memcpy(this, &y, sizeof(minimal_sso));
   #endif
    y.init_to_local_empty();
  }
  minimal_sso& operator=(minimal_sso&& y) {
    if (terark_likely(this != &y)) {
      this->~minimal_sso();
      new(this)minimal_sso(std::move(y));
    }
    return *this;
  }
  void swap(minimal_sso& y) { // this impl should be fastest
   #if defined(__GNUC__) && !defined(__clang__)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wclass-memaccess"
   #endif
    char tmp[sizeof(minimal_sso)];
    memcpy(tmp, this, sizeof(minimal_sso));
    memcpy(this,  &y, sizeof(minimal_sso));
    memcpy(&y,   tmp, sizeof(minimal_sso));
   #if defined(__GNUC__) && !defined(__clang__)
    #pragma GCC diagnostic pop
   #endif
  }

  void shrink_to_fit() {
    if (m_local.m_unused_len != 255) {
      TERARK_ASSERT_LE(m_local.m_unused_len, sizeof(m_local.m_space));
      // do nothing
    } else {
      shrink_to_fit_alloc();
    }
  }
private:
  terark_no_inline void shrink_to_fit_alloc() {
    size_t len = m_alloc.m_len;
    if (len <= sizeof(m_local.m_space)) { // localize
      char* ptr = m_alloc.m_ptr;
      memcpy(m_local.m_space, ptr, len + WithEOS);
      m_local.m_unused_len = sizeof(m_local.m_space) - len;
      free(ptr);
    } else {
      size_t cap = pow2_align_up(m_alloc.m_len + 1, 16);
      if (cap - 1 < m_alloc.m_cap) {
        m_alloc.m_ptr = (char*)realloc(m_alloc.m_ptr, cap);
        m_alloc.m_cap = cap - 1;
        TERARK_VERIFY(nullptr != m_alloc.m_ptr);
      }
    }
  }

public:
  void clear() {
    if (m_local.m_unused_len != 255) { // local
      TERARK_ASSERT_LE(m_local.m_unused_len, sizeof(m_local.m_space));
      init_to_local_empty();
    } else {
      m_alloc.m_len = 0;
      if (WithEOS)
        m_alloc.m_ptr[0] = '\0';
    }
  }
  void destroy() {
    this->~minimal_sso();
    init_to_local_empty();
  }
  terark_flatten void reserve(size_t cap) {
    if (m_local.m_unused_len != 255) { // local
      TERARK_ASSERT_LE(m_local.m_unused_len, sizeof(m_local.m_space));
      if (cap > sizeof(m_local.m_space)) {
        size_t len = local_size();
        cap = pow2_align_up(cap + 1, 64);
        char* ptr = (char*)malloc(cap);
        memcpy(ptr, m_local.m_space, len);
        if (WithEOS)
          ptr[len] = '\0';
        m_alloc.m_ptr = ptr;
        m_alloc.m_len = len;
        m_alloc.m_cap = cap - 1;
        m_alloc.m_flag = 255;
      }
    } else {
      if (cap > m_alloc.m_cap) {
        cap = pow2_align_up(cap + 1, 64);
        m_alloc.m_ptr = (char*)realloc(m_alloc.m_ptr, cap);
        m_alloc.m_cap = cap - 1;
        TERARK_VERIFY(nullptr != m_alloc.m_ptr);
      }
    }
  }
  ///@returns oldsize
  terark_flatten size_t resize(size_t newsize, char ch = '\0') {
    return resize(newsize, UninitializedFillN<char>{ch});
  }
  template<class DataPopulator>
  auto resize(size_t newsize, DataPopulator populate) ->
  decltype((populate((char*)0, size_t(1)), size_t(1))) {
    size_t oldsize;
    if (m_local.m_unused_len != 255) { // local
      TERARK_ASSERT_LE(m_local.m_unused_len, sizeof(m_local.m_space));
      oldsize = sizeof(m_local.m_space) - m_local.m_unused_len;
      if (newsize <= oldsize) {
        if (WithEOS)
          m_local.m_space[newsize] = '\0';
        m_local.m_unused_len = sizeof(m_local.m_space) - newsize;
        return oldsize;
      }
    } else {
      oldsize = m_alloc.m_len;
      if (newsize <= oldsize) {
        m_alloc.m_len = newsize;
        if (WithEOS)
          m_alloc.m_ptr[newsize] = '\0';
        return oldsize;
      }
    }
    TERARK_ASSERT_GT(newsize, oldsize);
    append(newsize - oldsize, populate);
    return oldsize;
  }
  template <class Tstring>
  auto assign(const Tstring& s) -> std::enable_if_t
  <std::is_same_v<decltype(s.data()), const char*>, void>
  { assign(s.data(), s.size()); }
  void assign(const fstring  s) { assign(s.p, s.n); }
  void assign(const char* s) { assign(s, strlen(s)); }
  void assign(const char* s, size_t n) { assign(n, UninitializedCopyN<char>{s}); }
  void assign(size_t n, char ch) { assign(n, UninitializedFillN<char>{ch}); }
  template<class DataPopulator>
  auto assign(size_t n, DataPopulator populate) -> // SFINAE void
  decltype((populate((char*)0, size_t(1)), void(0))) {
    if (terark_likely(m_local.m_unused_len != 255)) {  // local
      TERARK_ASSERT_LE(m_local.m_unused_len, sizeof(m_local.m_space));
      new(this)minimal_sso(n, populate);
    } else {
      assign_slow_path(n, populate);
    }
  }
private:
  template<class DataPopulator>
  terark_no_inline
  void assign_slow_path(size_t n, DataPopulator populate) {
    if (terark_likely(m_alloc.m_cap < n)) {
      free(m_alloc.m_ptr);
      new(this)minimal_sso(n, populate);
    } else {
      m_alloc.m_len = n;
      if (WithEOS)
        m_alloc.m_ptr[n] = '\0';
      populate(m_alloc.m_ptr, n);
    }
  }

public:
  template<class DataPopulator>
  void risk_assign_local(size_t n, DataPopulator populate) {
    TERARK_ASSERT_LE(m_local.m_unused_len, sizeof(m_local.m_space));
    TERARK_ASSERT_LE(n, sizeof(m_local.m_space));
    m_local.m_unused_len = sizeof(m_local.m_space) - n;
    populate(m_local.m_space, n);
    if (WithEOS)
      m_local.m_space[n] = '\0';
  }

public:
  template <class Tstring>
  auto append(const Tstring& s) -> std::enable_if_t
  <std::is_same_v<decltype(s.data()), const char*>, void>
  { append(s.data(), s.size()); }
  void append(const fstring  s) { append(s.p, s.n); }
  terark_flatten
  void append(const char* s, size_t n) { append(n, UninitializedCopyN<char>{s}); }
  terark_flatten
  void append(size_t n, char ch) { append(n, UninitializedFillN<char>{ch}); }
  template<class DataPopulator>
  auto append(size_t addsize, DataPopulator populate) -> // SFINAE void
  decltype((populate((char*)0, size_t(1)), void(0))) {
    if (terark_likely(m_local.m_unused_len != 255)) {  // local
      TERARK_ASSERT_LE(m_local.m_unused_len, sizeof(m_local.m_space));
      size_t oldsize = local_size();
      size_t newsize = oldsize + addsize;
      if (newsize <= sizeof(m_local.m_space)) {
        m_local.m_unused_len = sizeof(m_local.m_space) - newsize;
        if (WithEOS)
          m_local.m_space[newsize] = '\0';
        populate(m_local.m_space + oldsize, addsize);
        TERARK_ASSERT_EQ(local_size(), newsize);
      } else {
        malloc_append(addsize, populate, oldsize, newsize);
      }
    } else {
      size_t oldsize = m_alloc.m_len;
      size_t newsize = oldsize + addsize;
      if (terark_likely(newsize <= m_alloc.m_cap)) {
        populate(m_alloc.m_ptr + oldsize, addsize);
        if (WithEOS)
          m_alloc.m_ptr[newsize] = '\0';
        m_alloc.m_len = newsize;
      } else {
        realloc_append(addsize, populate, oldsize, newsize);
      }
    }
  }
  terark_flatten
  void emplace_back(char c) { append(1, c); }
  void push_back(char c) { emplace_back(c); }
  void pop_back() {
    assert(!empty());
    if (m_local.m_unused_len != 255) { // local
        m_local.m_unused_len++;
        if (WithEOS)
          m_local.m_space[local_size()] = '\0';
    } else {
        m_alloc.m_len--;
        if (WithEOS)
          m_alloc.m_ptr[m_alloc.m_len] = '\0';
    }
  }
  bool empty() const {
    if (m_local.m_unused_len != 255) { // local
      TERARK_ASSERT_LE(m_local.m_unused_len, sizeof(m_local.m_space));
      return m_local.m_unused_len == sizeof(m_local.m_space);
    } else {
      return m_alloc.m_len == 0;
    }
  }
  bool is_local() const { return m_local.m_unused_len != 255; }
  bool is_local_full() const { // rarely used
    TERARK_ASSERT_LE(m_local.m_unused_len, sizeof(m_local.m_space));
    return m_local.m_unused_len == 0;
  }
  bool needs_alloc(size_t addsize) const {
    if (m_local.m_unused_len != 255) { // local
      TERARK_ASSERT_LE(m_local.m_unused_len, sizeof(m_local.m_space));
      return addsize > m_local.m_unused_len;
    } else {
      return addsize + m_alloc.m_len > m_alloc.m_cap;
    }
  }
  size_t capacity() const {
    if (m_local.m_unused_len != 255) {
      TERARK_ASSERT_LE(m_local.m_unused_len, sizeof(m_local.m_space));
      return sizeof(m_local.m_space);
    } else {
      return m_alloc.m_cap;
    }
  }
  size_t length() const { return size(); }
  size_t size() const {
    if (m_local.m_unused_len != 255) {
      TERARK_ASSERT_LE(m_local.m_unused_len, sizeof(m_local.m_space));
      return local_size();
    } else {
      return m_alloc.m_len;
    }
  }
  char* data() {
    if (m_local.m_unused_len != 255) {
      TERARK_ASSERT_LE(m_local.m_unused_len, sizeof(m_local.m_space));
      return m_local.m_space;
    } else {
      return m_alloc.m_ptr;
    }
  }
  const char* data() const {
    if (m_local.m_unused_len != 255) {
      TERARK_ASSERT_LE(m_local.m_unused_len, sizeof(m_local.m_space));
      return m_local.m_space;
    } else {
      return m_alloc.m_ptr;
    }
  }
  char* begin() { return data(); }
  const char* begin() const { return data(); }
  char* end() {
    if (m_local.m_unused_len != 255) {
      TERARK_ASSERT_LE(m_local.m_unused_len, sizeof(m_local.m_space));
      return m_local.m_space + local_size();
    } else {
      return m_alloc.m_ptr + m_alloc.m_len;
    }
  }
  const char* end() const {
    if (m_local.m_unused_len != 255) {
      TERARK_ASSERT_LE(m_local.m_unused_len, sizeof(m_local.m_space));
      return m_local.m_space + local_size();
    } else {
      return m_alloc.m_ptr + m_alloc.m_len;
    }
  }
  template<class Tstring>
  Tstring to() const {
    if (m_local.m_unused_len != 255) {
      TERARK_ASSERT_LE(m_local.m_unused_len, sizeof(m_local.m_space));
      return Tstring(m_local.m_space, local_size());
    } else {
      return Tstring(m_alloc.m_ptr, m_alloc.m_len);
    }
  }
  template<class Tstring = minimal_sso>
  Tstring substr(size_t pos, size_t len) const {
    if (m_local.m_unused_len != 255) {
      TERARK_ASSERT_LE(m_local.m_unused_len, sizeof(m_local.m_space));
      TERARK_ASSERT_LE(pos, local_size());
      TERARK_ASSERT_LE(pos + len, local_size());
      return Tstring(m_local.m_space + pos, len);
    } else {
      TERARK_ASSERT_LE(pos, m_alloc.m_len);
      TERARK_ASSERT_LE(pos + len, m_alloc.m_len);
      return Tstring(m_alloc.m_ptr + pos, len);
    }
  }
  template<class Tstring = minimal_sso>
  Tstring substr(size_t pos) const {
    if (m_local.m_unused_len != 255) {
      TERARK_ASSERT_LE(m_local.m_unused_len, sizeof(m_local.m_space));
      TERARK_ASSERT_LE(pos, local_size());
      return Tstring(m_local.m_space + pos, local_size() - pos);
    } else {
      TERARK_ASSERT_LE(pos, m_alloc.m_len);
      return Tstring(m_alloc.m_ptr + pos, m_alloc.m_len - pos);
    }
  }
  template<class Tstring = minimal_sso>
  Tstring prefix(size_t len) const { // Tstring(data(), len) with assert check
    if (m_local.m_unused_len != 255) {
      TERARK_ASSERT_LE(m_local.m_unused_len, sizeof(m_local.m_space));
      TERARK_ASSERT_LE(len, local_size());
      return Tstring(m_local.m_space, len);
    } else {
      TERARK_ASSERT_LE(len, m_alloc.m_len);
      return Tstring(m_alloc.m_ptr, len);
    }
  }
  template<class Tstring = minimal_sso>
  Tstring suffix(size_t len) const {
    if (m_local.m_unused_len != 255) {
      TERARK_ASSERT_LE(m_local.m_unused_len, sizeof(m_local.m_space));
      TERARK_ASSERT_LE(len, local_size());
      return Tstring(m_local.m_space + local_size() - len, len);
    } else {
      TERARK_ASSERT_LE(len, m_alloc.m_len);
      return Tstring(m_alloc.m_ptr + m_alloc.m_len - len, len);
    }
  }
  template<class Tstring = minimal_sso>
  Tstring notail(size_t len) const {
    if (m_local.m_unused_len != 255) {
      TERARK_ASSERT_LE(m_local.m_unused_len, sizeof(m_local.m_space));
      TERARK_ASSERT_LE(len, local_size());
      return Tstring(m_local.m_space, local_size() - len);
    } else {
      TERARK_ASSERT_LE(len, m_alloc.m_len);
      return Tstring(m_alloc.m_ptr, m_alloc.m_len - len);
    }
  }
  const char* c_str() const { assert('\0' == *end()); return data(); }
  std::string str() const { return to<std::string>(); }

  template <class Tstring>
  auto operator+=(const Tstring& y) -> std::enable_if_t
  <std::is_same_v<decltype(y.data()), const char*>, minimal_sso&>
  { append(y); return *this; }
  minimal_sso& operator+=(const fstring& y) { append(y); return *this; }
  minimal_sso& operator+=(const minimal_sso& y) {
    append(y.to<fstring>());
    return *this;
  }

  static minimal_sso concat(const fstring& x, const fstring& y) {
    minimal_sso z(x.size() + y.size(), valvec_reserve());
    z.assign(x);
    z.append(y);
    return z;
  }
  friend minimal_sso operator+(const fstring& x, const minimal_sso& y) {
    return concat(x, y.template to<fstring>());
  }
  friend minimal_sso operator+(const minimal_sso& x, const fstring& y) {
    return concat(x.template to<fstring>(), y);
  }
  friend minimal_sso operator+(const minimal_sso& x, const minimal_sso& y) {
    return concat(x.template to<fstring>(), y.template to<fstring>());
  }
  friend minimal_sso operator+(const minimal_sso& x, const char* y) {
    return concat(x.template to<fstring>(), fstring(y));
  }
  friend minimal_sso operator+(const char* x, const minimal_sso& y) {
    return concat(fstring(x), y.template to<fstring>());
  }

  int compare(const minimal_sso& y) const {
    return this->template to<fstring>().compare(y.template to<fstring>());
  }

 #if defined(_MSC_VER)
  #define TERARK_MINIMAL_SSO_CMP_OPERATOR(op) \
    friend bool operator op (const minimal_sso& x, const minimal_sso& y) \
    { return x.to<fstring>() op y.to<fstring>(); }
    //~~~~~~~~~~~~~~~~~~~~
 #else
  #define TERARK_MINIMAL_SSO_CMP_OPERATOR(op) \
    friend bool operator op (const minimal_sso& x, const minimal_sso& y) \
    { return x.to<fstring>() op y.to<fstring>(); } \
    friend bool operator op (const minimal_sso& x, const fstring& y) \
    { return x.to<fstring>() op y; } \
    friend bool operator op (const fstring& x, const minimal_sso& y) \
    { return x op y.to<fstring>(); } \
    //~~~~~~~~~~~~~~~~~~~~
 #endif

  TERARK_MINIMAL_SSO_CMP_OPERATOR(==)
  TERARK_MINIMAL_SSO_CMP_OPERATOR(!=)
  TERARK_MINIMAL_SSO_CMP_OPERATOR(<)
  TERARK_MINIMAL_SSO_CMP_OPERATOR(<=)
  TERARK_MINIMAL_SSO_CMP_OPERATOR(>)
  TERARK_MINIMAL_SSO_CMP_OPERATOR(>=)
};
#pragma pack(pop)

} // namespace terark

namespace std {

template <size_t SizeSSO, bool WithEOS>
void swap(terark::minimal_sso<SizeSSO, WithEOS>& x,
          terark::minimal_sso<SizeSSO, WithEOS>& y) { x.swap(y); }

} // namespace std

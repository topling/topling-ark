
#include <terark/vec_trb.hpp>
#include <terark/vec_trb_hash.hpp>

#include <chrono>
#include <cstring>
#include <functional>
#include <iostream>
#include <random>
#include <set>
#include <string>
#include <unordered_set>
#include <stdio.h>

template<class... A> void reserve_cap(std::map<A...>&, size_t) {}
template<class... A> void reserve_cap(std::set<A...>&, size_t) {}

template<class... A> void reserve_cap(std::multimap<A...>&, size_t) {}
template<class... A> void reserve_cap(std::multiset<A...>&, size_t) {}

template<class... A> void reserve_cap(std::unordered_map<A...>&, size_t) {}
template<class... A> void reserve_cap(std::unordered_set<A...>&, size_t) {}

template<class... A> void reserve_cap(std::unordered_multimap<A...>&, size_t) {}
template<class... A> void reserve_cap(std::unordered_multiset<A...>&, size_t) {}

template<class VecLike>
void reserve_cap(VecLike& vec, size_t cap) { vec.reserve(cap); }

template <size_t key_size>
struct key {
  union {
    int32_t value;
    char data[key_size];
  };
  key() = default;
  key(key const &) = default;
  key(int32_t v) : value(v) {}
  operator int32_t() const { return value; }
  key &operator=(key const &) = default;
  bool operator<(key const &f) const { return value < f.value; }
};

template <size_t N>
struct std::hash<key<N>> {
  size_t operator()(key<N> const &key) const {
    return std::hash<int32_t>()(key.value);
  }
};

std::vector<size_t> make_test(size_t size, size_t split) {
  std::vector<size_t> ret;
  for (size_t i = 5; i < 5 + split; ++i) {
    ret.push_back(size_t(round(pow(size, 1.0 / (5 + split) * (i + 1)))));
  }
  return ret;
}

template <class T, class C, size_t N>
struct test_core {
  test_core(std::string const &_info, T const *_data[], size_t _length,
            std::function<void(C &, T const *, size_t)> _prepare,
            std::function<size_t(C &, T const *, size_t)> _run)
      : info(_info),
        data(_data),
        length(_length),
        prepare(_prepare),
        run(_run) {}
  std::string info;
  T const **data;
  size_t length;
  std::function<void(C &, T const *, size_t)> prepare;
  std::function<size_t(C &, T const *, size_t)> run;
  struct time_span_t {
    std::chrono::high_resolution_clock::time_point begin;
    std::chrono::high_resolution_clock::time_point end;
  };
  std::vector<time_span_t> time[N];

  void exec(size_t split) {
    auto count = make_test(length, split);
    for (size_t i = 0; i < N; ++i) {
      for (size_t max : count) {
        time_span_t t;
        size_t test_len = std::min(max, length);
        C c;
        prepare(c, data[i], test_len);
        t.begin = std::chrono::high_resolution_clock::now();
        run(c, data[i], test_len);
        t.end = std::chrono::high_resolution_clock::now();
        time[i].push_back(t);
      }
    }
  }
};

template <class T, class C, size_t N>
class test_all {
 public:
  test_all(std::string const &_info, T const *_data[N], size_t _length)
      : info(_info), data(_data), length(_length) {
    test.emplace_back(
        "insert_o", data, length, [](C &c, T const *, size_t e) {},
        [](C &c, T const *d, size_t e) {
          reserve_cap(c, e);
          for (size_t i = 0; i < e; ++i) {
            c.emplace(int32_t(i));
          }
          return 0;
        });
    test.emplace_back(
        "insert_r", data, length, [](C &c, T const *, size_t e) {},
        [](C &c, T const *d, size_t e) {
          reserve_cap(c, e);
          for (size_t i = 0; i < e; ++i) {
            c.emplace(d[i]);
          }
          return 0;
        });
    test.emplace_back(
        "foreach", data, length,
        [](C &c, T const *d, size_t e) {
          reserve_cap(c, e);
          for (size_t i = 0; i < e; ++i) {
            c.emplace(d[i]);
          }
        },
        [](C &c, T const *d, size_t e) {
          size_t sum = 0;
          for (auto &item : c) {
            sum += item;
          }
          return sum;
        });
    test.emplace_back(
        "find", data, length,
        [](C &c, T const *d, size_t e) {
          reserve_cap(c, e);
          for (size_t i = 0; i < e; ++i) {
            c.emplace(d[i]);
          }
        },
        [](C &c, T const *d, size_t e) {
          size_t sum = 0;
          for (size_t i = 0; i < e; ++i) {
            sum += *c.find(d[i]);
          }
          return sum;
        });
    test.emplace_back(
        "erase", data, length,
        [](C &c, T const *d, size_t e) {
          reserve_cap(c, e);
          for (size_t i = 0; i < e; ++i) {
            c.emplace(d[i]);
          }
        },
        [](C &c, T const *d, size_t e) {
          for (size_t i = 0; i < e; ++i) {
            c.erase(d[i]);
          }
          return 0;
        });
    foo = [](int *ptr) { delete[] ptr; };
  }

  void exec(size_t split) {
    for (auto &item : test) {
      foo(new int[1024]);
      printf("%s | %s", info.c_str(), item.info.c_str());
      item.exec(split);
      auto count = make_test(length, split);
      for (size_t j = 0; j < count.size(); ++j) {
        std::vector<std::chrono::high_resolution_clock::duration> value;
        double s = 0, v = 0;
        for (size_t i = 0; i < N; ++i) {
          value.push_back(item.time[i][j].end - item.time[i][j].begin);
          s += value.back().count();
        }
        s /= N;
        for (size_t i = 0; i < N; ++i) {
          v += std::pow(s - value[i].count(), 2);
        }
        v = std::sqrt(v);
        std::sort(
            value.begin(), value.end(),
            [&](std::chrono::high_resolution_clock::duration const left,
                std::chrono::high_resolution_clock::duration const &right) {
              return std::abs(left.count() - s) < std::abs(right.count() - s);
            });
        if (s < v) {
          value.pop_back();
        }
        std::chrono::high_resolution_clock::duration d =
            std::chrono::high_resolution_clock::duration();
        for (auto vi : value) {
          d += vi;
        }
        printf(" | %8.3f", std::chrono::duration_cast<
                         std::chrono::duration<float, std::nano>>(d)
                             .count() /
                         value.size() / count[j]);
      }
      printf("\n");
    }
  }

  std::vector<test_core<T, C, N>> test;
  std::function<void(int *)> foo;

 private:
  std::string info;
  T const **data;
  size_t length;
};

template <class T>
void foo_test_t(T &bp) {
  typedef typename std::remove_const<T>::type O;
  typename O::key_compare c;
  typename O::vec_type a;
  typename O::storage_type v{};
  typename O::storage_type const cv = v;
  typename O::key_type k;
  O o, oo;
  auto b = bp.cbegin();
  auto e = bp.cend();
  T o00(c);
  T o01(a);
  T o02(c, a);
  T o03(b, e, c);
  T o04(b, e, a);
  T o05(b, e, c, a);
  T o06(o);
  T o07(o, a);
  T o08(std::move(o));
  T o09(std::move(o), a);
  T o10({}, c);
  T o11({}, a);
  T o12({}, c, a);
  o = oo;
  o = std::move(oo);
  o = {};
  o.swap(oo);
  o.insert(v);
  o.insert(cv);
  o.insert(std::move(v));
  o.insert(o.begin(), v);
  o.insert(o.begin(), cv);
  o.insert(o.begin(), std::move(v));
  o.insert(b, e);
  o.insert({});
  o.emplace(v);
  o.emplace(cv);
  o.emplace_hint(o.begin(), v);
  o.emplace_hint(o.begin(), cv);
  bp.find(k);
  o.erase(o.cbegin());
  o.erase(k);
  o.erase(b, e);
  bp.count(k);
  bp.lower_bound(k);
  bp.upper_bound(k);
  bp.equal_range(k);
  bp.begin();
  bp.cbegin();
  bp.rbegin();
  bp.crbegin();
  bp.end();
  bp.cend();
  bp.rend();
  bp.crend();
  bp.empty();
  o.empty();
  o.size();
  o.max_size();
  bp.beg_i();
  bp.end_i();
  bp.rbeg_i();
  bp.rend_i();
  bp.next_i(0);
  bp.prev_i(0);
  bp.key(0);
  bp.elem_at(0);
}

template <class T>
void foo_test_h(T &hs) {
  typedef typename std::remove_const<T>::type O;
  typename O::hasher h;
  typename O::key_compare ke;
  typename O::allocator_type a;
  typename O::storage_type v{};
  typename O::storage_type const cv = v;
  typename O::key_type k;
  O o, oo;
  auto b = hs.cbegin();
  auto e = hs.cend();
  auto lb = hs.cbegin(0);
  auto le = hs.cend(0);
  T o00(0, h, ke, a);
  T o01(a);
  T o02(0, a);
  T o03(0, h, a);
  T o04(b, e, 0, h, ke, a);
  T o05(b, e, 0, a);
  T o06(b, e, 0, h, a);
  T o07(o);
  T o08(o, a);
  T o09(std::move(o));
  T o10(std::move(o), a);
  T o11({}, 0, h, ke, a);
  T o12({}, 0, a);
  T o13({}, 0, h, a);
  o = oo;
  o = std::move(oo);
  o = {};
  o.swap(oo);
  o.insert(v);
  o.insert(cv);
  o.insert(std::move(v));
  o.insert(o.begin(), v);
  o.insert(o.begin(), cv);
  o.insert(o.begin(), std::move(v));
  o.insert(b, e);
  o.insert({});
  o.emplace(v);
  o.emplace(cv);
  o.emplace_hint(o.begin(), v);
  o.emplace_hint(o.begin(), cv);
  hs.find(k);
  o.erase(o.cbegin());
  o.erase(k);
  o.erase(b, e);
  o.erase(lb);
  o.erase(lb, le);
  hs.count(k);
  hs.equal_range(k);
  hs.begin();
  hs.cbegin();
  hs.end();
  hs.cend();
  hs.empty();
  o.empty();
  o.size();
  o.max_size();
  while (lb != le) {
    ++lb;
  }
  hs.bucket_count();
  hs.max_bucket_count();
  hs.bucket_size(0);
  hs.bucket(k);
  o.reserve(0);
  o.rehash(0);
  o.max_load_factor(0);
  o.max_load_factor();
  o.load_factor();
}

void foo() {
  vec_trb_map<int, int> t_0;
  vec_trb_map<std::string, std::string> t_1;
  vec_trb_map<int, int> const t_2;
  vec_trb_map<std::string, std::string> const t_3;
  vec_trb_multimap<int, int> t_4;
  vec_trb_multimap<std::string, std::string> t_5;
  vec_trb_multimap<int, int> const t_6;
  vec_trb_multimap<std::string, std::string> const t_7;
  vec_trb_set<int> t_8;
  vec_trb_set<std::string> t_9;
  vec_trb_set<int> const t_a;
  vec_trb_set<std::string> const t_b;
  vec_trb_multiset<int> t_c;
  vec_trb_multiset<std::string> t_d;
  vec_trb_multiset<int> const t_e;
  vec_trb_multiset<std::string> const t_f;

  t_0[1] = 1;
  t_1["1"] = "1";

  foo_test_t(t_0);
  foo_test_t(t_1);
  foo_test_t(t_2);
  foo_test_t(t_3);
  foo_test_t(t_4);
  foo_test_t(t_5);
  foo_test_t(t_6);
  foo_test_t(t_7);
  foo_test_t(t_8);
  foo_test_t(t_9);
  foo_test_t(t_a);
  foo_test_t(t_b);
  foo_test_t(t_c);
  foo_test_t(t_d);
  foo_test_t(t_e);
  foo_test_t(t_f);

  vec_trb_hash_map<int, int> h_0;
  vec_trb_hash_map<std::string, std::string> h_1;
  vec_trb_hash_map<int, int> const h_2;
  vec_trb_hash_map<std::string, std::string> const h_3;
  vec_trb_hash_multimap<int, int> h_4;
  vec_trb_hash_multimap<std::string, std::string> h_5;
  vec_trb_hash_multimap<int, int> const h_6;
  vec_trb_hash_multimap<std::string, std::string> const h_7;
  vec_trb_hash_set<int> h_8;
  vec_trb_hash_set<std::string> h_9;
  vec_trb_hash_set<int> const h_a;
  vec_trb_hash_set<std::string> const h_b;
  vec_trb_hash_multiset<int> h_c;
  vec_trb_hash_multiset<std::string> h_d;
  vec_trb_hash_multiset<int> const h_e;
  vec_trb_hash_multiset<std::string> const h_f;

  h_0[1] = 1;
  h_1["1"] = "1";

  foo_test_h(h_0);
  foo_test_h(h_1);
  foo_test_h(h_2);
  foo_test_h(h_3);
  foo_test_h(h_4);
  foo_test_h(h_5);
  foo_test_h(h_6);
  foo_test_h(h_7);
  foo_test_h(h_8);
  foo_test_h(h_9);
  foo_test_h(h_a);
  foo_test_h(h_b);
  foo_test_h(h_c);
  foo_test_h(h_d);
  foo_test_h(h_e);
  foo_test_h(h_f);
}

int main() {
  if (([]{return false;})()) {
    foo();
  }

  std::mt19937 mt(0);
  auto mtr = std::uniform_int_distribution<int>(-200000000, 200000000);
  static constexpr size_t count = TERARK_IF_DEBUG(4000, 400000);
  static constexpr size_t times = TERARK_IF_DEBUG(2, 10);
  static constexpr size_t split = 17;
  std::vector<int> v_arr[times];

  for (auto &v : v_arr) {
    v.resize(count);
    for (auto &value : v) {
      value = mtr(mt);
    }
  }
  int const *v[times];
  for (size_t i = 0; i < times; ++i) {
    v[i] = v_arr[i].data();
  }

  // generate markdown output
  printf("container | keylen | method ");
  for (auto item : make_test(count, split)) {
    printf(" | %zd", item);
  }
  printf("\n");
  printf("-------|-------:|-------");
  for (auto n = (int)make_test(count, split).size(), i = 0; i < n; i++) {
    printf("|-------:");
  }
  printf("\n");

 #define vec_trb_mset  vec_trb_multiset
 #define vec_trb_hash  vec_trb_hash_set
 #define vec_trb_mhash vec_trb_hash_multiset
 #define std_cpp_set   std::set
 #define std_cpp_mset  std::multiset
 #define std_cpp_hash  std::unordered_set
 #define std_cpp_mhash std::unordered_multiset
  test_all<int, vec_trb_set  <key<  4> >, times>("trb_set   |   4 ", v, count).exec(split);
  test_all<int, vec_trb_hash <key<  4> >, times>("trb_hash  |   4 ", v, count).exec(split);
  test_all<int, std_cpp_set  <key<  4> >, times>("std_set   |   4 ", v, count).exec(split);
  test_all<int, std_cpp_hash <key<  4> >, times>("std_hash  |   4 ", v, count).exec(split);
  test_all<int, vec_trb_set  <key< 32> >, times>("trb_set   |  32 ", v, count).exec(split);
  test_all<int, vec_trb_hash <key< 32> >, times>("trb_hash  |  32 ", v, count).exec(split);
  test_all<int, std_cpp_set  <key< 32> >, times>("std_set   |  32 ", v, count).exec(split);
  test_all<int, std_cpp_hash <key< 32> >, times>("std_hash  |  32 ", v, count).exec(split);
  test_all<int, vec_trb_set  <key< 64> >, times>("trb_set   |  64 ", v, count).exec(split);
  test_all<int, vec_trb_hash <key< 64> >, times>("trb_hash  |  64 ", v, count).exec(split);
  test_all<int, std_cpp_set  <key< 64> >, times>("std_set   |  64 ", v, count).exec(split);
  test_all<int, std_cpp_hash <key< 64> >, times>("std_hash  |  64 ", v, count).exec(split);
  test_all<int, vec_trb_set  <key<128> >, times>("trb_set   | 128 ", v, count).exec(split);
  test_all<int, vec_trb_hash <key<128> >, times>("trb_hash  | 128 ", v, count).exec(split);
  test_all<int, std_cpp_set  <key<128> >, times>("std_set   | 128 ", v, count).exec(split);
  test_all<int, std_cpp_hash <key<128> >, times>("std_hash  | 128 ", v, count).exec(split);
  test_all<int, vec_trb_set  <key<200> >, times>("trb_set   | 200 ", v, count).exec(split);
  test_all<int, vec_trb_hash <key<200> >, times>("trb_hash  | 200 ", v, count).exec(split);
  test_all<int, std_cpp_set  <key<200> >, times>("std_set   | 200 ", v, count).exec(split);
  test_all<int, std_cpp_hash <key<200> >, times>("std_hash  | 200 ", v, count).exec(split);
  test_all<int, vec_trb_mset <key<  4> >, times>("trb_mset  |   4 ", v, count).exec(split);
  test_all<int, vec_trb_mhash<key<  4> >, times>("trb_mhash |   4 ", v, count).exec(split);
  test_all<int, std_cpp_mset <key<  4> >, times>("std_mset  |   4 ", v, count).exec(split);
  test_all<int, std_cpp_mhash<key<  4> >, times>("std_mhash |   4 ", v, count).exec(split);
  test_all<int, vec_trb_mset <key< 32> >, times>("trb_mset  |  32 ", v, count).exec(split);
  test_all<int, vec_trb_mhash<key< 32> >, times>("trb_mhash |  32 ", v, count).exec(split);
  test_all<int, std_cpp_mset <key< 32> >, times>("std_mset  |  32 ", v, count).exec(split);
  test_all<int, std_cpp_mhash<key< 32> >, times>("std_mhash |  32 ", v, count).exec(split);
  test_all<int, vec_trb_mset <key< 64> >, times>("trb_mset  |  64 ", v, count).exec(split);
  test_all<int, vec_trb_mhash<key< 64> >, times>("trb_mhash |  64 ", v, count).exec(split);
  test_all<int, std_cpp_mset <key< 64> >, times>("std_mset  |  64 ", v, count).exec(split);
  test_all<int, std_cpp_mhash<key< 64> >, times>("std_mhash |  64 ", v, count).exec(split);
  test_all<int, vec_trb_mset <key<128> >, times>("trb_mset  | 128 ", v, count).exec(split);
  test_all<int, vec_trb_mhash<key<128> >, times>("trb_mhash | 128 ", v, count).exec(split);
  test_all<int, std_cpp_mset <key<128> >, times>("std_mset  | 128 ", v, count).exec(split);
  test_all<int, std_cpp_mhash<key<128> >, times>("std_mhash | 128 ", v, count).exec(split);
  test_all<int, vec_trb_mset <key<200> >, times>("trb_mset  | 200 ", v, count).exec(split);
  test_all<int, vec_trb_mhash<key<200> >, times>("trb_mhash | 200 ", v, count).exec(split);
  test_all<int, std_cpp_mset <key<200> >, times>("std_mset  | 200 ", v, count).exec(split);
  test_all<int, std_cpp_mhash<key<200> >, times>("std_mhash | 200 ", v, count).exec(split);
}

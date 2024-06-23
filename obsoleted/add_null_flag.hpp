#pragma once
#include <boost/type_traits/has_trivial_destructor.hpp>
#include <utility>

template<class T, bool HasTrivialDestructor>
struct add_null_flag_aux {
    unsigned char storage[sizeof(T)] = {0};
    bool is_constructed = false;
    ~add_null_flag_aux() {
        if (is_constructed)
            reinterpret_cast<T*>(storage)->~T();
    }
};
template<class T>
struct add_null_flag_aux<T, true> {
    unsigned char storage[sizeof(T)] = {0};
    bool is_constructed = false;
};
template<class T>
class add_null_flag
 : private add_null_flag_aux<T, boost::has_trivial_destructor<T>::value> {
public:
    bool is_null() const { return !this->is_constructed; }
    template<class... ArgList>
    void construct(ArgList&&... args) {
        assert(!this->is_constructed);
        ::new(this->storage) T (std::forward<ArgList>(args)...);
        this->is_constructed = true;
    }
    const
    T& get_readable() const { return *reinterpret_cast<const T*>(this->storage); }
    T& get_writable()       { return *reinterpret_cast<      T*>(this->storage); }
};


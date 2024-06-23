/* vim: set tabstop=4 : */
#ifndef __terark_shadow_vector_h__
#define __terark_shadow_vector_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#include <assert.h>
#include <boost/current_function.hpp>
#include <boost/iterator.hpp>

namespace terark {

//////////////////////////////////////////////////////////////////////////

template<class T, class Ptr, class Ref, class Diff>
class reverse_pointer :
	public boost::random_access_iterator_helper<
			reverse_pointer<T, Ref, Ptr, Diff>,
			T, Diff, Ptr, Ref
		>
{
	typedef reverse_pointer<T, Ref, Ptr, Diff> my_type;

public:
	explicit reverse_pointer(Ptr p) : m_p(p - 1) {}

	//! 比起 std::reverse_iterator<Ptr> 的优势所在，dereference 时不需要 --m_p
	Ref operator* () const { return *m_p; }
	Ptr operator->() const { return  m_p; }

	Ptr base() const { return m_p + 1; }

	my_type& operator++() { --m_p; return *this; }
	my_type& operator--() { ++m_p; return *this; }

	my_type& operator+=(Diff diff) { m_p -= diff; return *this; }
	my_type& operator-=(Diff diff) { m_p += diff; return *this; }

	friend Diff operator-(my_type x, my_type y)
	{
		return y.m_p - x.m_p;
	}

	friend bool operator<(my_type x, my_type y)
	{
		return y.m_p < x.m_p;
	}
	friend bool operator==(my_type x, my_type y)
	{
		return x.m_p == y.m_p;
	}

private:
	T* m_p;
};

template<class T> class shadow_vector
{
	T *m_beg, *m_end;

public:
	typedef T		  value_type;
	typedef T&		  reference;
	typedef T*		  iterator;
	typedef const T*  const_iterator;
	typedef size_t    size_type;
	typedef ptrdiff_t difference_type;
	typedef reverse_pointer<T, T*, T&, ptrdiff_t> reverse_iterator;
	typedef reverse_pointer<T, const T*, const T&, ptrdiff_t> const_reverse_iterator;

	shadow_vector(iterator first, iterator last)
		: m_beg(first), m_end(last)
	{
		assert(first <= last);
	}

	shadow_vector(iterator first, size_t count)
		: m_beg(first), m_end(first + count)
	{
		assert(count >= 0);
	}

	void init(iterator first, iterator last)
	{
		assert(first <= last);
		m_beg = (first), m_end = (last);
	}

	void init(iterator first, size_t count)
	{
		assert(count >= 0);
		m_beg = (first), m_end = (first + count);
	}

	iterator begin() { return m_beg; }
	iterator end()   { return m_end; }

	const_iterator begin() const { return m_beg; }
	const_iterator end()   const { return m_end; }

	reverse_iterator rbegin() { return const_iterator(m_end); }
	reverse_iterator rend()   { return const_iterator(m_beg); }

	const_reverse_iterator rbegin() const { return const_iterator(m_end); }
	const_reverse_iterator rend()   const { return const_iterator(m_beg); }

	size_type size()     const { return m_end - m_beg; }
	size_type capacity() const { return m_end - m_beg; }

	T& operator[](difference_type idx)
	{
		assert(m_beg + idx < m_end);
		return m_beg[idx];
	}
	const T& operator[](difference_type idx) const
	{
		assert(m_beg + idx < m_end);
		return m_beg[idx];
	}

	T& at(difference_type idx)
	{
		if (m_beg + idx < m_end)
			return m_beg[idx];
		ThrowOutOfRange(idx, BOOST_CURRENT_FUNCTION);
	}
	const T& at(difference_type idx) const
	{
		if (m_beg + idx < m_end)
			return m_beg[idx];
		ThrowOutOfRange(idx, BOOST_CURRENT_FUNCTION);
	}

private:
	void ThrowOutOfRange(difference_type idx, const char* func) const
	{
		string_appender<> oss;
		oss << "out of range, subscript = " << idx << ", in function:\n"
			<< func;
		throw std::out_of_range(oss.str());
	}
};

template<class T> class pseudo_vector
{
	size_t	m_size;
	size_t  m_capacity;

public:
	typedef T		  value_type;
	typedef T&		  reference;
	typedef T*		  iterator;
	typedef const T*  const_iterator;
	typedef size_t    size_type;
	typedef ptrdiff_t difference_type;

	pseudo_vector(iterator first, iterator last) { }
	pseudo_vector(iterator first, size_type count) { }

	iterator begin() { return 0; }
	iterator end()   { return (T*)(m_size); }

	const_iterator begin() const { return 0; }
	const_iterator end()   const { return (T*)(m_size); }

	size_type size()     const { return m_size; }
	size_type capacity() const { return m_capacity; }

	void reserve(size_type newCapacity)
	{
		if (newCapacity >= m_size)
			m_capacity = newCapacity;
	}

	T& operator[](difference_type idx)
	{
		assert(idx < difference_type(m_size));
		return begin()[idx];
	}
	const T& operator[](difference_type idx) const
	{
		assert(idx < difference_type(m_size));
		return begin()[idx];
	}

	T& at(difference_type idx)
	{
		if (idx < difference_type(m_size))
			return (*this)[idx];
		ThrowOutOfRange(idx, BOOST_CURRENT_FUNCTION);
	}
	const T& at(difference_type idx) const
	{
		if (idx < difference_type(m_size))
			return (*this)[idx];
		ThrowOutOfRange(idx, BOOST_CURRENT_FUNCTION);
	}

private:
	void ThrowOutOfRange(difference_type idx, const char* func) const
	{
		string_appender<> oss;
		oss << "out of range, subscript = " << idx << ", in function:\n"
			<< func;
		throw std::out_of_range(oss.str());
	}
};

} // namespace terark

#endif // __terark_shadow_vector_h__


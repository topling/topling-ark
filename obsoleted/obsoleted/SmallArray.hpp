/* vim: set tabstop=4 : */
#ifndef SmallArray_h__
#define SmallArray_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#include <terark/stdtypes.hpp>
#include "io/DataIO.hpp"

namespace terark {

//@{
//! if T of SmallArray<T, MaxSize> is not primitive type
//! or your end mark is not 0, please write your special
//! SmallArray_ElementIsNull/SmallArray_ElementIsNull,
//! before your calling of SmallArray<T, MaxSize>.
template<class T>
bool SmallArray_ElementIsNull(const T& e) { return 0 == e; }
template<class T>
void SmallArray_ElementSetNull(T& e) { e = 0; }
//@}

//! if the array was not reached MaxSize, it ended with a NULL value..
//! if 'MaxSize' is greater than 8, do not use SmallArray, it is low performance!!
template<class T, int MaxSize>
class SmallArray
{
	T data[MaxSize];

public:
	typedef T  value_type;
	typedef T& reference;
	typedef T* iterator;
	typedef const T* const_iterator;
	typedef const T& const_reference;
	typedef int size_type;

public:
	SmallArray() { clear(); }

	//! complexity: O(n)
	size_type size() const
	{
		for (int i = 0; i < MaxSize; ++i)
		{
			if (SmallArray_ElementIsNull(data[i]))
				return i;
		}
		return MaxSize;
	}

	//! complexity: O(1)
	size_type capacity() const { return MaxSize; }
	bool empty() const { return SmallArray_ElementIsNull(data[0]); }

	//! complexity: O(1)
	iterator	   begin()		 { return data; }
	const_iterator begin() const { return data; }

	//! complexity: O(n)
	iterator	   end()	   { return data + size(); }
	const_iterator end() const { return data + size(); }

	//! complexity: O(n)
	bool push_back(const T& e)
	{
		size_type n = size();
		if (n < MaxSize) {
			data[n] = e;
			return true;
		}
		return false;
	}

	//! complexity: O(n)
	bool pop_back()
	{
		size_type n = size();
		if (n > 0) {
			SmallArray_ElementSetNull(data[n - 1]);
			return true;
		}
		return false;
	}

	//! complexity: O(n)
	bool pop_back(T& x)
	{
		size_type n = size();
		if (n > 0) {
			x = data[n - 1];
			SmallArray_ElementSetNull(data[n - 1]);
			return true;
		}
		return false;
	}

	//! complexity: O(n)
	//! this will cause element's movement
	void erase(iterator iter)
	{
		assert(begin() <= iter && iter < end());

		int i = iter - begin();
		while (!SmallArray_ElementIsNull(*iter)) {
			if (MaxSize - 1 == i) {
				SmallArray_ElementSetNull(*iter);
				break;
			} else {
				iter[0] = iter[1];
				++iter; ++i;
			}
		}
		return;
	}

	//! complexity: O(n)
	//! this will cause element's movement
	void erase(int nth)
	{
		assert(0 <= nth && nth < MaxSize);

		while (!SmallArray_ElementIsNull(data[nth])) {
			if (MaxSize - 1 == nth) {
				SmallArray_ElementSetNull(data[nth]);
				break;
			} else {
				data[nth] = data[nth + 1];
				++nth;
			}
		}
		return;
	}

	//! complexity: O(1)
	void clear()
	{
		SmallArray_ElementSetNull(data[0]);
	}

	//! complexity: O(1)
	const T& front() const { return data[0]; }
	T& front() { return data[0]; }

	//! complexity: O(n)
	const T& back() const
	{
		size_type n = size();
		if (n > 0)
			return data[n-1];
		return data[0];
	}

	//! complexity: O(n)
	T& back()
	{
		size_type n = size();
		if (n > 0)
			return data[n-1];
		return data[0];
	}

	//! complexity: O(1)
	T& operator[](int nth)
	{
		assert(0 <= nth && nth < MaxSize);
		return data[nth];
	}

	//! complexity: O(1)
	const T& operator[](int nth) const
	{
		assert(0 <= nth && nth < MaxSize);
		return data[nth];
	}

	//! complexity: O(1)
	void set_null(int nth)
	{
		assert(0 <= nth && nth < MaxSize);
		SmallArray_ElementSetNull(data[nth]);
	}
private:
	//@{
	//! serialization...
	//!
	//! size is saved as uint16_t....
	//!
	template<class Input> void dio_load(Input& in)
	{
		uint16_t size;
		in >> size;
		SizeValueTooLargeException::checkSizeValue(size, MaxSize);
		for (uint16_t i = 0; i < size; ++i)
		{
			in >> data[i];
		}
		if (size < MaxSize)
			SmallArray_ElementSetNull(data[size]);
	}
	template<class Output> void dio_save(Output& out) const
	{
		uint16_t size = (uint16_t)this->size();
		out << size;
		for (uint16_t i = 0; i < size; ++i)
			out << data[i];
	}
	DATA_IO_REG_LOAD_SAVE(SmallArray)
	//@}
};

} // namespace terark

#endif // SmallArray_h__


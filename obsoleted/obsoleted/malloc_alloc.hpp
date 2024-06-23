// xmemory internal header (from <memory>)
#ifndef __TERARK_MALLOC_ALLOC__
#define __TERARK_MALLOC_ALLOC__

#ifdef  _MSC_VER
 #pragma once
 #include <xutility>

 #pragma pack(push,_CRT_PACKING)
 #pragma warning(push,3)
 #pragma warning(disable: 4100)
#else
/*
 #include <cstddef>

//# include <Boost/config.hpp>

 #if defined(__GNUC__)
 # include <stdint.h>
 #endif

 #if defined(linux) || defined(__linux) || defined(__linux__)
 # include <linux/types.h>
 #else
 # include <sys/types.h>
 #endif
*/
#endif  /* _MSC_VER */

 #include <cstdlib>
 #include <exception>
 #include <new>

#ifdef _FARQ	/* specify standard memory model */
 #define _TERARK_FARQ _FARQ
 #define _TERARK_PDFT _PDFT
 #define _TERARK_SIZT _SIZT
#else
 #define _TERARK_FARQ
 #define _TERARK_PDFT	ptrdiff_t
 #define _TERARK_SIZT	size_t
#endif /* _FARQ */

namespace terark {

		// TEMPLATE FUNCTION TERARK_Destroy
template<class _Ty> inline
	void TERARK_Destroy(_Ty _TERARK_FARQ *_Ptr)
	{	// destroy object at _Ptr
	 _Ptr->~_Ty();
	}

inline
	void TERARK_Destroy(char _TERARK_FARQ *)
	{	// destroy a char (do nothing)
	}

inline
	void TERARK_Destroy(wchar_t _TERARK_FARQ *)
	{	// destroy a wchar_t (do nothing)
	}

		// TEMPLATE CLASS _Allocator_base
template<class _Ty>
	struct _Allocator_base
	{	// base class for generic allocators
	typedef _Ty value_type;
	};

		// TEMPLATE CLASS _Allocator_base<const _Ty>
template<class _Ty>
	struct _Allocator_base<const _Ty>
	{	// base class for generic allocators for const _Ty
	typedef _Ty value_type;
	};

		// TEMPLATE CLASS malloc_alloc
template<class _Ty>
	class malloc_alloc
		: public _Allocator_base<_Ty>
	{	// generic malloc_alloc for objects of class _Ty
public:
	typedef _Allocator_base<_Ty> _Mybase;
	typedef typename _Mybase::value_type value_type;
	typedef value_type _TERARK_FARQ *pointer;
	typedef value_type _TERARK_FARQ& reference;
	typedef const value_type _TERARK_FARQ *const_pointer;
	typedef const value_type _TERARK_FARQ& const_reference;

	typedef _TERARK_SIZT size_type;
	typedef _TERARK_PDFT difference_type;

	template<class _Other>
		struct rebind
		{	// convert an malloc_alloc<_Ty> to an malloc_alloc <_Other>
		typedef malloc_alloc<_Other> other;
		};

	pointer address(reference _Val) const
		{	// return address of mutable _Val
		return (&_Val);
		}

	const_pointer address(const_reference _Val) const
		{	// return address of nonmutable _Val
		return (&_Val);
		}

	malloc_alloc() throw()
		{	// construct default malloc_alloc (do nothing)
		}

	malloc_alloc(const malloc_alloc<_Ty>&) throw()
		{	// construct by copying (do nothing)
		}

	template<class _Other>
		malloc_alloc(const malloc_alloc<_Other>&) throw()
		{	// construct from a related malloc_alloc (do nothing)
		}

	template<class _Other>
		malloc_alloc<_Ty>& operator=(const malloc_alloc<_Other>&)
		{	// assign from a related malloc_alloc (do nothing)
		return (*this);
		}

	void deallocate(pointer _Ptr, size_type _Count)
		{	// deallocate object at _Ptr, ignore _Count
	//	::operator delete(_Ptr);
		::free(_Ptr);
		}

	pointer allocate(size_type _Count)
		{	// allocate array of _Count elements
		if (_Count <= 0)
			_Count = 0;
		else if (((_TERARK_SIZT)(-1) / _Count) < sizeof (_Ty))
			throw std::bad_alloc();

			// allocate storage for _Count elements of type _Ty
	//	return ((_Ty _TERARK_FARQ *)::operator new(_Count * sizeof (_Ty)));
		return ((_Ty _TERARK_FARQ *)::malloc(_Count * sizeof (_Ty)));
		}

	pointer allocate(size_type _Count, const void _TERARK_FARQ *)
		{	// allocate array of _Count elements, ignore hint
		return (allocate(_Count));
		}

	void construct(pointer _Ptr, const _Ty& _Val)
		{	// construct object at _Ptr with value _Val
		::new (_Ptr) _Ty(_Val);
		}

	void destroy(pointer _Ptr)
		{	// destroy object at _Ptr
		TERARK_Destroy(_Ptr);
		}

	_TERARK_SIZT max_size() const throw()
		{	// estimate maximum array size
		_TERARK_SIZT _Count = (_TERARK_SIZT)(-1) / sizeof (_Ty);
		return (0 < _Count ? _Count : 1);
		}
	};

		// malloc_alloc TEMPLATE OPERATORS
template<class _Ty,
	class _Other> inline
	bool operator==(const malloc_alloc<_Ty>&, const malloc_alloc<_Other>&) throw()
	{	// test for malloc_alloc equality (always true)
	return (true);
	}

template<class _Ty,
	class _Other> inline
	bool operator!=(const malloc_alloc<_Ty>&, const malloc_alloc<_Other>&) throw()
	{	// test for malloc_alloc inequality (always false)
	return (false);
	}

		// CLASS malloc_alloc<void>
template<> class malloc_alloc<void>
	{	// generic malloc_alloc for type void
public:
	typedef void _Ty;
	typedef _Ty _TERARK_FARQ *pointer;
	typedef const _Ty _TERARK_FARQ *const_pointer;
	typedef _Ty value_type;

	template<class _Other>
		struct rebind
		{	// convert an malloc_alloc<void> to an malloc_alloc <_Other>
		typedef malloc_alloc<_Other> other;
		};

	malloc_alloc() throw()
		{	// construct default malloc_alloc (do nothing)
		}

	malloc_alloc(const malloc_alloc<_Ty>&) throw()
		{	// construct by copying (do nothing)
		}

	template<class _Other>
		malloc_alloc(const malloc_alloc<_Other>&) throw()
		{	// construct from related malloc_alloc (do nothing)
		}

	template<class _Other>
		malloc_alloc<_Ty>& operator=(const malloc_alloc<_Other>&)
		{	// assign from a related malloc_alloc (do nothing)
		return (*this);
		}
	};

} // namespace terark

#ifdef _MSC_VER
 #pragma warning(default: 4100)
 #pragma warning(pop)
 #pragma pack(pop)
#endif  /* _MSC_VER */

#endif /* __TERARK_MALLOC_ALLOC__ */

/*
 * Copyright (c) 1992-2007 by P.J. Plauger.  ALL RIGHTS RESERVED.
 * Consult your license regarding permissions and restrictions.
 */

/*
 * This file is derived from software bearing the following
 * restrictions:
 *
 * Copyright (c) 1994
 * Hewlett-Packard Company
 *
 * Permission to use, copy, modify, distribute and sell this
 * software and its documentation for any purpose is hereby
 * granted without fee, provided that the above copyright notice
 * appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation.
 * Hewlett-Packard Company makes no representations about the
 * suitability of this software for any purpose. It is provided
 * "as is" without express or implied warranty.
 V5.03:0009 */

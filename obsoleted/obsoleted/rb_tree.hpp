/*
 *
 * Copyright (c) 1996,1997
 * Silicon Graphics Computer Systems, Inc.
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation.  Silicon Graphics makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 *
 * Copyright (c) 1994
 * Hewlett-Packard Company
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation.  Hewlett-Packard Company makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 *
 */

/* NOTE: This is an internal header file, included by other STL headers.
 *   You should not attempt to use it directly.
 */

#ifndef __TERARK_STL_INTERNAL_TREE_H
#define __TERARK_STL_INTERNAL_TREE_H

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
# pragma warning(disable: 4127)
#endif

#include <assert.h>
#include <iterator>
#include <cstddef>

/*
#if defined(_MSC_VER) && _MSC_VER > 1400
# include <xutility>
# define MS_CONTAINER_BASE_AUX_ALLOC(x) _CONTAINER_BASE_AUX_ALLOC<x>
#else
# define MS_CONTAINER_BASE_AUX_ALLOC(x) x
#endif
*/
//# include <Boost/config.hpp>

#if defined(__GNUC__)
# include <stdint.h>
#endif

#if defined(linux) || defined(__linux) || defined(__linux__)
# include <linux/types.h>
#else
# include <sys/types.h>
#endif

#define TERARK_RB_TREE_CLASS_PARTIAL_SPECIALIZATION

/*

Red-black tree class, designed for use in implementing STL
associative containers (set, multiset, map, and multimap). The
insertion and deletion algorithms are based on those in Cormen,
Leiserson, and Rivest, Introduction to Algorithms (MIT Press, 1990),
except that

(1) the header cell is maintained with links not only to the root
but also to the leftmost node of the tree, to enable constant time
begin(), and to the rightmost node of the tree, to enable linear time
performance when used with the generic set algorithms (set_union,
etc.);

(2) when a node being deleted has two children its successor node is
relinked into its place, rather than copied, so that the only
iterators invalidated are those referring to the deleted node.

*/

//#include <terark/config.hpp>
//#include <stl_algobase.h>
//#include <stl_alloc.h>
//#include <stl_construct.h>
//#include <stl_function.h>

//TERARK_RB_TREE_BEGIN_NAMESPACE

#define TERARK_RB_TREE_FUNCTION_TMPL_PARTIAL_ORDER
#define TERARK_RB_TREE_MEMBER_TEMPLATES
#define TERARK_RB_TREE_DEFAULT_ALLOCATOR(x) std::allocator<x>
#define TERARK_RB_TREE_TRY try
#define TERARK_RB_TREE_CATCH_ALL catch(...)
#define TERARK_RB_TREE_THROW(x) throw x
#define TERARK_RB_TREE_RETHROW throw
#define TERARK_RB_TREE_NOTHROW throw()
#define TERARK_RB_TREE_UNWIND(action) catch(...) { action; throw; }

#if defined(sgi) && !defined(__GNUC) && (_MIPS_SIM != _MIPS_SIM_ABI32)
#pragma set woff 1375
#endif

//   static _Terark_Rb_tree_node_base* _S_node(char* base, int node_offset)
//   {
// 	  return (_Terark_Rb_tree_node_base*)(base + node_offset);
//   }
//   static _Terark_Rb_tree_node_base*& _S_left(char* base, int node_offset)
//   {
// 	  return ((_Terark_Rb_tree_node_base*)(base + node_offset))->left;
//   }
//   static _Terark_Rb_tree_node_base*& _S_right(char* base, int node_offset)
//   {
// 	  return ((_Terark_Rb_tree_node_base*)(base + node_offset))->right;
//   }
//   static uintptr_t& _S_parent(char* base, int node_offset)
//   {
// 	  return ((_Terark_Rb_tree_node_base*)(base + node_offset))->links[0];
//   }

struct _Terark_Rb_tree_node_base
{
	//! [31,30....2,1,0], 1 is lock, 0 is color
	uintptr_t links[1]; // (parent,lock,color)
	_Terark_Rb_tree_node_base *left, *right;

	_Terark_Rb_tree_node_base* getparent() const
	{
		// lowest 2 bits are extra info
		return (_Terark_Rb_tree_node_base*)(links[0] & ~(uintptr_t)(3));
	}

	void setparent(_Terark_Rb_tree_node_base* p)
	{
	//	links[0] = links[0] & (uintptr_t)(1) | (uintptr_t)(p) & ~(uintptr_t)(1);
		links[0] = links[0] & (uintptr_t)(1) | (uintptr_t)(p); // not need '& ~(uintptr_t)(1)';
	}

	uintptr_t isRed()   const { return   links[0] & (uintptr_t)(1); }
	uintptr_t isblack() const { return !(links[0] & (uintptr_t)(1)); }

	void setRed()   { links[0] |= 1; }
	void setblack() { links[0] &= ~(uintptr_t)(1); }

	void swapcolor(_Terark_Rb_tree_node_base* y)
	{
		uintptr_t tcolor = y->links[0] & (uintptr_t)(1);
		y->links[0] = y->links[0] & ~(uintptr_t)(1) | links[0] & (uintptr_t)(1);
		links[0] = links[0] & ~(uintptr_t)(1) | tcolor;
	}

	void copycolor(const _Terark_Rb_tree_node_base* y)
	{
		links[0] = links[0] & ~(uintptr_t)(1) | y->links[0] & (uintptr_t)(1);
	}

//   _Terark_Rb_tree_node_base* getLeft() const
//   {
// 	  return links[1] & ~(uintptr_t)(1);
//   }
//   _Terark_Rb_tree_node_base* getRight() const
//   {
// 	  return links[2] & ~(uintptr_t)(1);
//   }

	static _Terark_Rb_tree_node_base* _S_minimum(_Terark_Rb_tree_node_base* x)
	{
		while (x->left != 0)
            x = x->left;
		return x;
	}

	static _Terark_Rb_tree_node_base* _S_maximum(_Terark_Rb_tree_node_base* x)
	{
		while (x->right != 0)
            x = x->right;
		return x;
	}
};

template<class _Value>
struct _Terark_Rb_tree_node : public _Terark_Rb_tree_node_base
{
  typedef _Terark_Rb_tree_node<_Value>* _Link_type;
  _Value value_field;
};

struct _Terark_Rb_tree_base_iterator
{
  typedef std::bidirectional_iterator_tag iterator_category;
  typedef ptrdiff_t difference_type;
  _Terark_Rb_tree_node_base* node;

  void increment();
  void decrement();
};

template <class _Value, class _Ref, class _Ptr>
struct _Terark_Rb_tree_iterator : public _Terark_Rb_tree_base_iterator
{
  typedef _Value value_type;
  typedef _Ref reference;
  typedef _Ptr pointer;
  typedef _Terark_Rb_tree_iterator<_Value, _Value&, _Value*>
    iterator;
  typedef _Terark_Rb_tree_iterator<_Value, const _Value&, const _Value*>
    const_iterator;
  typedef _Terark_Rb_tree_iterator<_Value, _Ref, _Ptr>
    _Self;
  typedef _Terark_Rb_tree_node<_Value>* _Link_type;

  _Terark_Rb_tree_iterator() {}
  _Terark_Rb_tree_iterator(_Link_type x) { node = x; }
  _Terark_Rb_tree_iterator(const iterator& it) { node = it.node; }

  reference operator*() const { return _Link_type(node)->value_field; }
  pointer operator->() const { return &_Link_type(node)->value_field; }

  _Self& operator++() { increment(); return *this; }
  _Self operator++(int) {
    _Self tmp = *this;
    increment();
    return tmp;
  }

  _Self& operator--() { decrement(); return *this; }
  _Self operator--(int) {
    _Self tmp = *this;
    decrement();
    return tmp;
  }
};

inline bool operator==(const _Terark_Rb_tree_base_iterator& x,
                       const _Terark_Rb_tree_base_iterator& y) {
  return x.node == y.node;
}

inline bool operator!=(const _Terark_Rb_tree_base_iterator& x,
                       const _Terark_Rb_tree_base_iterator& y) {
  return x.node != y.node;
}

#ifndef TERARK_RB_TREE_CLASS_PARTIAL_SPECIALIZATION

inline std::bidirectional_iterator_tag
iterator_category(const _Terark_Rb_tree_base_iterator&) {
	return std::bidirectional_iterator_tag();
}

inline _Terark_Rb_tree_base_iterator::difference_type*
distance_type(const _Terark_Rb_tree_base_iterator&) {
  return (_Terark_Rb_tree_base_iterator::difference_type*) 0;
}

template <class _Value, class _Ref, class _Ptr>
inline _Value* value_type(const _Terark_Rb_tree_iterator<_Value, _Ref, _Ptr>&) {
  return (_Value*) 0;
}

#endif /* TERARK_RB_TREE_CLASS_PARTIAL_SPECIALIZATION */

void _Terark_Rb_tree_rotate_left(_Terark_Rb_tree_node_base* x, _Terark_Rb_tree_node_base*& root);


void _Terark_Rb_tree_rotate_right(_Terark_Rb_tree_node_base* x, _Terark_Rb_tree_node_base*& root);

void _Terark_Rb_tree_rebalance(_Terark_Rb_tree_node_base* x, _Terark_Rb_tree_node_base*& root);

_Terark_Rb_tree_node_base*
_Terark_Rb_tree_rebalance_for_erase(_Terark_Rb_tree_node_base* z,
                             _Terark_Rb_tree_node_base*& root,
                             _Terark_Rb_tree_node_base*& leftmost,
                             _Terark_Rb_tree_node_base*& rightmost);

// Base class to encapsulate the differences between old SGI-style
// allocators and standard-conforming allocators.  In order to avoid
// having an empty base class, we arbitrarily move one of rb_tree's
// data members into the base class.

template <class _Value, class _Alloc>
struct _Terark_Rb_tree_base : public _Alloc::template rebind<_Terark_Rb_tree_node<_Value> >::other
//	public MS_CONTAINER_BASE_AUX_ALLOC(_Alloc::template rebind<_Terark_Rb_tree_node<_Value> >::other)
{
  typedef typename _Alloc::template rebind<_Terark_Rb_tree_node<_Value> >::other _NodeAlloc;

  typedef _Alloc allocator_type;

  _Terark_Rb_tree_base(const allocator_type& = allocator_type())
    : header(0) { header = get_node(); }

  ~_Terark_Rb_tree_base() { put_node(header); }

  typedef typename _Alloc::template rebind<_Terark_Rb_tree_node<_Value> >::other _Alloc_type;

  _Terark_Rb_tree_node<_Value>* get_node() { return this->allocate(1); }
  void put_node(_Terark_Rb_tree_node<_Value>* p) { this->deallocate(p, 1); }

protected:
  _Terark_Rb_tree_node<_Value>* header;
};


template <class _Value, class _Alloc>
struct _Terark_Rb_tree_base_with_var_node : public _Alloc::template rebind<unsigned char>::other
//	public MS_CONTAINER_BASE_AUX_ALLOC(_Alloc::template rebind<unsigned char>::other)
{
  typedef _Alloc allocator_type;

  size_t total_node_size() const { return m_total_node_size; }

  size_t align_up(size_t size) const { return (size+sizeof(uintptr_t)-1 & ~(2*sizeof(uintptr_t)-1)) + sizeof(uintptr_t); }

protected:
  typedef typename _Alloc::template rebind<_Terark_Rb_tree_node<_Value> >::other _Alloc_type;

  _Terark_Rb_tree_base_with_var_node(const allocator_type& = allocator_type())
    : header(0), m_total_node_size(0)
  {
	  header = get_node(sizeof(uintptr_t));
  }

  ~_Terark_Rb_tree_base_with_var_node()
  {
	  put_node(header, sizeof(uintptr_t));
	  assert(0 == m_total_node_size);
  }

  _Terark_Rb_tree_node<_Value>* get_node(size_t data_size)
  {
//	  assert(align_up(data_size) == data_size);
	  _Terark_Rb_tree_node<_Value>* node =
	 (_Terark_Rb_tree_node<_Value>*)this->allocate(sizeof(_Terark_Rb_tree_node_base) + data_size);
	  m_total_node_size += sizeof(_Terark_Rb_tree_node_base) + data_size;
	  return node;
  }
  void put_node(_Terark_Rb_tree_node<_Value>* p, size_t data_size)
  {
	  this->deallocate((unsigned char*)p, sizeof(_Terark_Rb_tree_node_base) + data_size);
	  m_total_node_size -= sizeof(_Terark_Rb_tree_node_base) + data_size;
  }

protected:
  _Terark_Rb_tree_node<_Value>* header;
  size_t m_total_node_size;
};

template <class _Key, class _Value, class _KeyOfValue, class _Compare, class _Alloc, class _Base>
class _Terark_Rb_tree_common : public _Base
{
public:
  typedef _Key				key_type;
  typedef _Value			value_type;
  typedef value_type*		pointer;
  typedef const value_type* const_pointer;
  typedef value_type&		reference;
  typedef const value_type& const_reference;
  typedef size_t			size_type;
  typedef ptrdiff_t			difference_type;

  typedef _Terark_Rb_tree_node<_Value>*	_Link_type;
  typedef typename _Base::allocator_type allocator_type;

protected:
  using _Base::header;
  size_type   node_count; // keeps track of size of tree
  _KeyOfValue key_of_value;
  _Compare    key_compare;

  _Link_type root() const { return (_Link_type) header->getparent(); }

  void setroot(_Terark_Rb_tree_node_base* p) { header->setparent(p); }

  _Link_type& leftmost() const  { return (_Link_type&) header->left; }
  _Link_type& rightmost() const { return (_Link_type&) header->right; }

  static _Link_type& _S_left(_Link_type x)  { return (_Link_type&)(x->left); }
  static _Link_type& _S_left(_Terark_Rb_tree_node_base*  x)  { return (_Link_type&)(x->left); }

  static _Link_type& _S_right(_Link_type x) { return (_Link_type&)(x->right); }
  static _Link_type& _S_right(_Terark_Rb_tree_node_base*  x) { return (_Link_type&)(x->right); }

  static reference _S_value(_Link_type x) { return x->value_field; }
  static reference _S_value(_Terark_Rb_tree_node_base*  x) { return ((_Link_type)x)->value_field; }

  // maybe make a temp _Key to return
  // because key_of_value return a local object, use r-reference is better when C++0x
  const _Key key_of_node(_Link_type x) const { return key_of_value(x->value_field); }
  const _Key key_of_node(_Terark_Rb_tree_node_base*  x) const { return key_of_value(_Link_type(x)->value_field); }

public:
  typedef _Terark_Rb_tree_iterator<value_type, reference, pointer> iterator;
  typedef _Terark_Rb_tree_iterator<value_type, const_reference, const_pointer>
          const_iterator;

#ifdef TERARK_RB_TREE_CLASS_PARTIAL_SPECIALIZATION
  typedef std::reverse_iterator<const_iterator> const_reverse_iterator;
  typedef std::reverse_iterator<iterator> reverse_iterator;
#else /* TERARK_RB_TREE_CLASS_PARTIAL_SPECIALIZATION */
  typedef std::reverse_bidirectional_iterator<iterator, value_type, reference,
                                         difference_type>
          reverse_iterator;
  typedef std::reverse_bidirectional_iterator<const_iterator, value_type,
                                         const_reference, difference_type>
          const_reverse_iterator;
#endif /* TERARK_RB_TREE_CLASS_PARTIAL_SPECIALIZATION */

public:
                                // allocation/deallocation
  _Terark_Rb_tree_common()
    : //_Base(allocator_type()),
	 node_count(0), key_compare()
    { empty_initialize(); }

  _Terark_Rb_tree_common(const _KeyOfValue& key_of_val)
    : //_Base(allocator_type()),
	 node_count(0), key_of_value(key_of_val)
    { empty_initialize(); }

  _Terark_Rb_tree_common(const _KeyOfValue& key_of_val, const _Compare& comp)
    : //_Base(allocator_type()),
	 node_count(0), key_of_value(key_of_val), key_compare(comp)
    { empty_initialize(); }

  _Terark_Rb_tree_common(const _KeyOfValue& key_of_val, const _Compare& comp, const allocator_type& a)
    : _Base(a),
	 node_count(0), key_of_value(key_of_val), key_compare(comp)
    { empty_initialize(); }

  void clone(const _Terark_Rb_tree_common<_Key,_Value,_KeyOfValue,_Compare,_Alloc,_Base>& x)
  {
	  assert(this != &x);
	  node_count = 0;
	  key_compare = x.key_compare;
    if (x.root() == 0)
      empty_initialize();
    else {
      header->setRed();
      setroot(copy(x.root(), header));
      leftmost() = (_Link_type)_Terark_Rb_tree_node_base::_S_minimum(root());
      rightmost() = (_Link_type)_Terark_Rb_tree_node_base::_S_maximum(root());
    }
    node_count = x.node_count;
  }

  virtual ~_Terark_Rb_tree_common() { assert(0 == node_count); }

_Terark_Rb_tree_common<_Key,_Value,_KeyOfValue,_Compare,_Alloc,_Base>&
operator=(const _Terark_Rb_tree_common<_Key,_Value,_KeyOfValue,_Compare,_Alloc,_Base>& x)
{
// this form need more memory!
// _Terark_Rb_tree<_Key,_Value,_KeyOfValue,_Compare,_Alloc>(x).swap(*this);

  if (this != &x) {
					// Note that _Key may be a constant type.
    clear();
    node_count = 0;
    key_compare = x.key_compare;
    if (x.root() == 0) {
      setroot(0);
      leftmost() = header;
      rightmost() = header;
    }
    else {
      setroot(copy(x.root(), header));
      leftmost() = (_Link_type)_Terark_Rb_tree_node_base::_S_minimum(root());
      rightmost() = (_Link_type)_Terark_Rb_tree_node_base::_S_maximum(root());
      node_count = x.node_count;
    }
  }
  return *this;
}

private:
  void empty_initialize() {
    header->setRed(); // used to distinguish header from
                      // root, in iterator.operator++
    setroot(0);
    leftmost() = header;
    rightmost() = header;
  }

protected:
  _Link_type copy(_Link_type x, _Link_type p)
 {
// structural copy.  x and p must be non-null.
  _Link_type top = clone_node(x);
  top->setparent(p);

  TERARK_RB_TREE_TRY {
    if (x->right)
      top->right = copy(_S_right(x), top);
    p = top;
    x = _S_left(x);

    while (x != 0) {
      _Link_type y = clone_node(x);
      p->left = y;
      y->setparent(p);
      if (x->right)
        y->right = copy(_S_right(x), y);
      p = y;
      x = _S_left(x);
    }
  }
  TERARK_RB_TREE_UNWIND(erase(top));

  return top;
}

  void erase(_Link_type x)
{
  // erase without rebalancing
  while (x != 0) {
    erase(_S_right(x));
    _Link_type y = _S_left(x);
    destroy_node(x);
    x = y;
  }
}

size_type erase(const _Key& x)
{
  std::pair<iterator,iterator> p = equal_range(x);
  size_type n = std::distance(p.first, p.second);
  erase(p.first, p.second);
  return n;
}

void erase(iterator first, iterator last)
{
  if (first == begin() && last == end())
    clear();
  else
    while (first != last) erase(first++);
}

void erase(const _Key* first, const _Key* last)
{
  while (first != last) erase(*first++);
}

void erase(iterator position)
{
  _Terark_Rb_tree_node_base* hparent = header->getparent();
  _Link_type y =
    (_Link_type) _Terark_Rb_tree_rebalance_for_erase(position.node,
                                              hparent,
                                              header->left,
                                              header->right);
  header->setparent(hparent);
  destroy_node(y);
  --node_count;
}

  virtual _Link_type clone_node(_Link_type x) = 0;
  virtual void destroy_node(_Link_type x) = 0;

public:
                                // accessors:
  _Compare key_comp() const { return key_compare; }

  iterator begin() { return leftmost(); }
  const_iterator begin() const { return leftmost(); }
  iterator end() { return header; }
  const_iterator end() const { return header; }
  reverse_iterator rbegin() { return reverse_iterator(end()); }
  const_reverse_iterator rbegin() const { return const_reverse_iterator(end()); }
  reverse_iterator rend() { return reverse_iterator(begin()); }
  const_reverse_iterator rend() const { return const_reverse_iterator(begin()); }

  bool empty() const { return node_count == 0; }
  size_type size() const { return node_count; }
  size_type max_size() const { return size_type(-1); }

  void swap(_Terark_Rb_tree_common<_Key,_Value,_KeyOfValue,_Compare,_Alloc,_Base>& t)
  {
    std::swap(header, t.header);
    std::swap(node_count, t.node_count);
	std::swap(key_of_value, t.key_of_value);
    std::swap(key_compare, t.key_compare);
  }

  void clear() {
    if (node_count != 0) {
      erase(root());
      leftmost() = header;
      setroot(0);
      rightmost() = header;
      node_count = 0;
    }
  }

public:
                                // set operations:
  iterator find(const key_type& k)
{
  _Link_type y = header;      // Last node which is not less than k.
  _Link_type x = root();      // Current node.

  while (x != 0)
    if (!key_compare(key_of_node(x), k))
      y = x, x = _S_left(x);
    else
      x = _S_right(x);

  iterator j = iterator(y);
  return (j == end() || key_compare(k, key_of_node(j.node))) ?
     end() : j;
}

  const_iterator find(const key_type& k) const
{
  _Link_type y = header; /* Last node which is not less than k. */
  _Link_type x = root(); /* Current node. */

  while (x != 0) {
    if (!key_compare(key_of_node(x), k))
      y = x, x = _S_left(x);
    else
      x = _S_right(x);
  }
  const_iterator j = const_iterator(y);
  return (j == end() || key_compare(k, key_of_node(j.node))) ?
    end() : j;
}
  size_type count(const key_type& k) const
{
  std::pair<const_iterator, const_iterator> p = equal_range(k);
  size_type n = std::distance(p.first, p.second);
  return n;
}

  iterator lower_bound(const key_type& k)
{
  _Link_type y = header; /* Last node which is not less than k. */
  _Link_type x = root(); /* Current node. */

  while (x != 0)
    if (!key_compare(key_of_node(x), k))
      y = x, x = _S_left(x);
    else
      x = _S_right(x);

  return iterator(y);
}

  const_iterator lower_bound(const key_type& k) const
{
  _Link_type y = header; /* Last node which is not less than k. */
  _Link_type x = root(); /* Current node. */

  while (x != 0)
    if (!key_compare(key_of_node(x), k))
      y = x, x = _S_left(x);
    else
      x = _S_right(x);

  return const_iterator(y);
}

  iterator upper_bound(const key_type& k)
{
  _Link_type y = header; /* Last node which is greater than k. */
  _Link_type x = root(); /* Current node. */

   while (x != 0)
     if (key_compare(k, key_of_node(x)))
       y = x, x = _S_left(x);
     else
       x = _S_right(x);

   return iterator(y);
}

  const_iterator upper_bound(const key_type& k) const
{
  _Link_type y = header; /* Last node which is greater than k. */
  _Link_type x = root(); /* Current node. */

   while (x != 0)
     if (key_compare(k, key_of_node(x)))
       y = x, x = _S_left(x);
     else
       x = _S_right(x);

   return const_iterator(y);
}

  std::pair<iterator,iterator> equal_range(const key_type& k)
  {
  return std::pair<iterator,iterator>(lower_bound(k), upper_bound(k));
  }
  std::pair<const_iterator, const_iterator> equal_range(const key_type& k) const
  {
  return std::pair<const_iterator,const_iterator>(lower_bound(k), upper_bound(k));
  }
public:
                                // Debugging.
  bool rb_verify() const;
};

template< class _Key, class _Value, class _KeyOfValue
		, class _Compare = std::less<_Key>
		, class _Alloc = TERARK_RB_TREE_DEFAULT_ALLOCATOR(_Value)
		>
class _Terark_Rb_tree :
	public _Terark_Rb_tree_common<_Key, _Value, _KeyOfValue, _Compare, _Alloc, _Terark_Rb_tree_base<_Value, _Alloc> >
{
	typedef _Terark_Rb_tree_common<_Key, _Value, _KeyOfValue, _Compare, _Alloc, _Terark_Rb_tree_base<_Value, _Alloc> > _Base;

public:
	typedef typename _Base::iterator							 iterator;
	typedef typename _Base::const_iterator				   const_iterator;
	typedef typename _Base::reverse_iterator			 reverse_iterator;
	typedef typename _Base::const_reverse_iterator const_reverse_iterator;

// 	typedef typename _Base::pointer		pointer;
// 	typedef typename _Base::reference	reference;
// 	typedef typename _Base::value_type	value_type;

	typedef _Terark_Rb_tree_node<_Value>* _Link_type;

	using _Base::begin;
	using _Base::end;
	using _Base::rbegin;
	using _Base::rend;
	using _Base::empty;
	using _Base::size;
	using _Base::swap;

protected:
    using _Base::header;

	using _Base::node_count;
	using _Base::key_of_value;
	using _Base::key_compare;

	using _Base::root;
	using _Base::setroot;
	using _Base::leftmost;
	using _Base::rightmost;

	using _Base::_S_left;
	using _Base::_S_right;
	using _Base::_S_value;

	using _Base::key_of_node;

  _Link_type create_node(const _Value& x)
  {
    _Link_type tmp = this->get_node();
    TERARK_RB_TREE_TRY {
      new(&tmp->value_field)_Value(x);
    }
    TERARK_RB_TREE_UNWIND(this->put_node(tmp));
    return tmp;
  }

  _Link_type clone_node(_Link_type x)
  {
    _Link_type tmp = create_node(x->value_field);
    tmp->links[0] = x->links[0];
    tmp->left = 0;
    tmp->right = 0;
    return tmp;
  }

  void destroy_node(_Link_type p)
  {
    p->value_field.~_Value();
    this->put_node(p);
  }

public:
	~_Terark_Rb_tree() { this->clear(); }

	_Terark_Rb_tree() {}

  _Terark_Rb_tree(const _KeyOfValue& key_of_val)
	  : _Base(key_of_val)
  {
  }

  _Terark_Rb_tree(const _KeyOfValue& key_of_val, const _Compare& comp)
    : _Base(key_of_val, comp)
    { }

  _Terark_Rb_tree(const _KeyOfValue& key_of_val, const _Compare& comp, const _Alloc& a)
    : _Base(key_of_val, comp, a)
    { }

  _Terark_Rb_tree(const _Terark_Rb_tree& rhs) { this->clone(rhs); }

iterator insert_node(_Terark_Rb_tree_node_base* x_, _Terark_Rb_tree_node_base* y_, const _Value& v)
{
  _Link_type x = (_Link_type) x_;
  _Link_type y = (_Link_type) y_;
  _Link_type z;

  if (y == header || x != 0 ||
      key_compare(key_of_value(v), key_of_node(y))) {
    z = create_node(v);
    _S_left(y) = z;               // also makes leftmost() = z
                                      //    when y == header
    if (y == header) {
      setroot(z);
      rightmost() = z;
    }
    else if (y == leftmost())
      leftmost() = z;   // maintain leftmost() pointing to min node
  }
  else {
    z = create_node(v);
    _S_right(y) = z;
    if (y == rightmost())
      rightmost() = z;  // maintain rightmost() pointing to max node
  }
  z->setparent(y);
  _S_left(z) = 0;
  _S_right(z) = 0;
  _Terark_Rb_tree_node_base* hparent = header->getparent();
  _Terark_Rb_tree_rebalance(z, hparent);
  header->setparent(hparent);
  ++node_count;
  return iterator(z);
}
                                // insert/erase
std::pair<iterator,bool> insert_unique(const _Value& v)
{
  _Link_type y = header;
  _Link_type x = root();
  bool comp = true;
  while (x != 0) {
    y = x;
    comp = key_compare(key_of_value(v), key_of_node(x));
    x = comp ? _S_left(x) : _S_right(x);
  }
  iterator j = iterator(y);
  if (comp)
    if (j == begin())
      return std::pair<iterator,bool>(insert_node(0, y, v), true);
    else
      --j;
  if (key_compare(key_of_node(j.node), key_of_value(v)))
    return std::pair<iterator,bool>(insert_node(0, y, v), true);
  return std::pair<iterator,bool>(j, false);
}

iterator insert_equal(const _Value& v)
{
  _Link_type y = header;
  _Link_type x = root();
  while (x != 0) {
    y = x;
    x = key_compare(key_of_value(v), key_of_node(x)) ?
            _S_left(x) : _S_right(x);
  }
  return insert_node(0, y, v);
}

iterator insert_unique(iterator position, const _Value& v)
{
  if (position.node == header->left) { // begin()
    if (this->size() > 0 &&
        key_compare(key_of_value(v), key_of_node(position.node)))
      return insert_node(position.node, position.node, v);
    // first argument just needs to be non-null
    else
      return insert_unique(v).first;
  } else if (position.node == header) { // end()
    if (key_compare(key_of_node(rightmost()), key_of_value(v)))
      return insert_node(0, rightmost(), v);
    else
      return insert_unique(v).first;
  } else {
    iterator before = position;
    --before;
    if (key_compare(key_of_node(before.node), key_of_value(v))
        && key_compare(key_of_value(v), key_of_node(position.node))) {
      if (_S_right(before.node) == 0)
        return insert_node(0, before.node, v);
      else
        return insert_node(position.node, position.node, v);
    // first argument just needs to be non-null
    } else
      return insert_unique(v).first;
  }
}

iterator insert_equal(iterator position, const _Value& v)
{
  if (position.node == header->left) { // begin()
    if (this->size() > 0 &&
        !key_compare(key_of_node(position.node), key_of_value(v)))
      return insert_node(position.node, position.node, v);
    // first argument just needs to be non-null
    else
      return insert_equal(v);
  } else if (position.node == header) {// end()
    if (!key_compare(key_of_value(v), key_of_node(rightmost())))
      return insert_node(0, rightmost(), v);
    else
      return insert_equal(v);
  } else {
    iterator before = position;
    --before;
    if (!key_compare(key_of_value(v), key_of_node(before.node))
        && !key_compare(key_of_node(position.node), key_of_value(v))) {
      if (_S_right(before.node) == 0)
        return insert_node(0, before.node, v);
      else
        return insert_node(position.node, position.node, v);
    // first argument just needs to be non-null
    } else
      return insert_equal(v);
  }
}

#ifdef TERARK_RB_TREE_MEMBER_TEMPLATES

template <class _InputIterator>
void insert_unique(_InputIterator first, _InputIterator last)
{
  for ( ; first != last; ++first)
    insert_unique(*first);
}

template <class _InputIterator>
void insert_equal(_InputIterator first, _InputIterator last)
{
  for ( ; first != last; ++first)
    insert_equal(*first);
}
#else /* TERARK_RB_TREE_MEMBER_TEMPLATES */
void insert_unique(const_iterator first, const_iterator last)
{
  for ( ; first != last; ++first)
    insert_unique(*first);
}
void insert_unique(const _Value* first, const _Value* last)
{
  for ( ; first != last; ++first)
    insert_unique(*first);
}
void insert_equal(const_iterator first, const_iterator last)
{
  for ( ; first != last; ++first)
    insert_equal(*first);
}
void insert_equal(const _Value* first, const _Value* last)
{
  for ( ; first != last; ++first)
    insert_equal(*first);
}

#endif /* TERARK_RB_TREE_MEMBER_TEMPLATES */
};

template< class _Key, class _Value, class _KeyOfValue
		, class _Compare = std::less<_Key>
		, class _Alloc = TERARK_RB_TREE_DEFAULT_ALLOCATOR(_Value)
		>
class _Terark_Rb_tree_with_var_node :
	public _Terark_Rb_tree_common<_Key, _Value, _KeyOfValue, _Compare, _Alloc, _Terark_Rb_tree_base_with_var_node<_Value, _Alloc> >
{
	typedef _Terark_Rb_tree_common<_Key, _Value, _KeyOfValue, _Compare, _Alloc, _Terark_Rb_tree_base_with_var_node<_Value, _Alloc> > _Base;

public:
	typedef typename _Base::iterator							 iterator;
	typedef typename _Base::const_iterator				   const_iterator;
	typedef typename _Base::reverse_iterator			 reverse_iterator;
	typedef typename _Base::const_reverse_iterator const_reverse_iterator;

// 	typedef typename _Base::pointer		pointer;
// 	typedef typename _Base::reference	reference;
// 	typedef typename _Base::value_type	value_type;

	using _Base::begin;
	using _Base::end;
	using _Base::rbegin;
	using _Base::rend;
	using _Base::empty;
	using _Base::size;
	using _Base::swap;
	using _Base::align_up;

protected:
    using _Base::header;

	using _Base::node_count;
	using _Base::key_of_value;
	using _Base::key_compare;

	using _Base::root;
	using _Base::setroot;
	using _Base::leftmost;
	using _Base::rightmost;

	using _Base::_S_left;
	using _Base::_S_right;
	using _Base::_S_value;

	using _Base::key_of_node;

	typedef _Terark_Rb_tree_node<_Value>* _Link_type;

  _Link_type create_node(const _Key& key, size_t data_size)
  {
    data_size = align_up(data_size);
    _Link_type tmp = this->get_node(data_size);
    TERARK_RB_TREE_TRY {
      new(&tmp->value_field)_Value(key, data_size);
    }
    TERARK_RB_TREE_UNWIND(this->put_node(tmp, data_size));
    return tmp;
  }

  _Link_type clone_node(_Link_type x)
  {
    _Link_type tmp = create_node(key_of_value(x->value_field), x->value_field.data_size());
    tmp->links[0] = x->links[0];
    tmp->left = 0;
    tmp->right = 0;
    return tmp;
  }

  void destroy_node(_Link_type p)
  {
    size_t data_size = p->value_field.data_size();
    p->value_field.~_Value();
    this->put_node(p, data_size);
  }

  iterator insert_node(_Terark_Rb_tree_node_base* x_, _Terark_Rb_tree_node_base* y_, const _Key& key, size_t data_size)
{
  _Link_type x = (_Link_type) x_;
  _Link_type y = (_Link_type) y_;
  _Link_type z;

  if (y == header || x != 0 ||
	  key_compare(key, _Base::key_of_node(y))) {
    z = create_node(key, data_size);
    _Base::_S_left(y) = z;               // also makes leftmost() = z
                                      //    when y == header
    if (y == header) {
      setroot(z);
      rightmost() = z;
    }
    else if (y == leftmost())
      leftmost() = z;   // maintain leftmost() pointing to min node
  }
  else {
    z = create_node(key, data_size);
    _S_right(y) = z;
    if (y == rightmost())
      rightmost() = z;  // maintain rightmost() pointing to max node
  }
  z->setparent(y);
  _S_left(z) = 0;
  _S_right(z) = 0;
  _Terark_Rb_tree_node_base* hparent = header->getparent();
  _Terark_Rb_tree_rebalance(z, hparent);
  header->setparent(hparent);
  ++node_count;
  return iterator(z);
}

public:
  ~_Terark_Rb_tree_with_var_node() { this->clear(); }

  _Terark_Rb_tree_with_var_node() {}
  _Terark_Rb_tree_with_var_node(const _KeyOfValue& key_of_val)
	  : _Base(key_of_val)
  {
  }

  _Terark_Rb_tree_with_var_node(const _KeyOfValue& key_of_val, const _Compare& comp)
    : _Base(key_of_val, comp)
    { }

  _Terark_Rb_tree_with_var_node(const _KeyOfValue& key_of_val, const _Compare& comp, const _Alloc& a)
    : _Base(key_of_val, comp, a)
    { }

	_Terark_Rb_tree_with_var_node(const _Terark_Rb_tree_with_var_node& rhs) { this->clone(); }

                                // insert/erase
  std::pair<iterator,bool> insert_unique(const _Key& key, size_t data_size)
  {
  _Link_type y = header;
  _Link_type x = this->root();
  bool comp = true;
  while (x != 0) {
    y = x;
	comp = key_compare(key, _Base::key_of_node(x));
    x = comp ? _Base::_S_left(x) : _Base::_S_right(x);
  }
  iterator j = iterator(y);
  if (comp)
    if (j == begin())
      return std::pair<iterator,bool>(insert_node(x, y, key, data_size), true);
    else
      --j;
  if (key_compare(_Base::key_of_node(j.node), key))
    return std::pair<iterator,bool>(insert_node(x, y, key, data_size), true);
  return std::pair<iterator,bool>(j, false);
}

  iterator insert_equal(const _Key& key, size_t data_size)
{
  _Link_type y = header;
  _Link_type x = root();
  while (x != 0) {
    y = x;
    x = this->key_compare(key, _Base::key_of_node(x)) ?
            _Base::_S_left(x) : _Base::_S_right(x);
  }
  return insert_node(x, y, key, data_size);
}
  iterator insert_unique(iterator position, const _Key& key, size_t data_size)
{
  if (position.node == header->left) { // begin()
    if (this->size() > 0 &&
        key_compare(key, key_of_node(position.node)))
      return insert_node(position.node, position.node, key, data_size);
    // first argument just needs to be non-null
    else
      return insert_unique(key, data_size).first;
  } else if (position.node == header) { // end()
    if (key_compare(key_of_node(rightmost()), key))
      return insert_node(0, rightmost(), key, data_size);
    else
      return insert_unique(key, data_size).first;
  } else {
    iterator before = position;
    --before;
    if (key_compare(key_of_node(before.node), key)
        && key_compare(key, key_of_node(position.node))) {
      if (_S_right(before.node) == 0)
        return insert_node(0, before.node, key, data_size);
      else
        return insert_node(position.node, position.node, key, data_size);
    // first argument just needs to be non-null
    } else
      return insert_unique(key, data_size).first;
  }
}
  iterator insert_equal(iterator position, const _Key& key, size_t data_size)
{
  if (position.node == header->left) { // begin()
    if (this->size() > 0 &&
        !key_compare(key_of_node(position.node), key))
      return insert_node(position.node, position.node, key, data_size);
    // first argument just needs to be non-null
    else
      return insert_equal(key, data_size);
  } else if (position.node == header) {// end()
    if (!key_compare(key, key_of_node(rightmost())))
      return insert_node(0, rightmost(), key, data_size);
    else
      return insert_equal(key, data_size);
  } else {
    iterator before = position;
    --before;
    if (!key_compare(key, key_of_node(before.node))
        && !key_compare(key_of_node(position.node), key)) {
      if (_S_right(before.node) == 0)
        return insert_node(0, before.node, key, data_size);
      else
        return insert_node(position.node, position.node, key, data_size);
    // first argument just needs to be non-null
    } else
       return insert_equal(key, data_size);
  }
}

  // call _Value::_Value(const _Value& rhs, new_data_size)
  // no old_data_size provided
  void update(iterator& position, size_t new_data_size)
  {
	 new_data_size = align_up(new_data_size);
     _Link_type node = this->get_node(new_data_size);
     size_t old_data_size = _S_value(position.node).data_size();
	 assert(align_up(old_data_size) == old_data_size);
	 new(&_S_value(node))_Value(_S_value(position.node), new_data_size);
	 do_update(position, node, old_data_size, new_data_size);
  }

  // call _Value::_Value(const _Value& rhs, old_data_size, new_data_size)
  void update(iterator& position, size_t old_data_size, size_t new_data_size)
  {
	 assert(align_up(old_data_size) == old_data_size);
	 new_data_size = align_up(new_data_size);
     _Link_type node = this->get_node(new_data_size);
	 new(&_S_value(node))_Value(_S_value(position.node), old_data_size, new_data_size);
	 do_update(position, node, old_data_size, new_data_size);
  }

  // call _Value::_Value(const _Value& rhs, old_data_size, new_data_size)
  void update_2(iterator& position, size_t new_data_size)
  {
	 new_data_size = align_up(new_data_size);
     _Link_type node = this->get_node(new_data_size);
     size_t old_data_size = _S_value(position.node).data_size();
	 assert(align_up(old_data_size) == old_data_size);
	 new(&_S_value(node))_Value(_S_value(position.node), old_data_size, new_data_size);
	 do_update(position, node, old_data_size, new_data_size);
  }
private:
  void do_update(iterator& position, _Link_type node, size_t old_data_size, size_t new_data_size)
  {
	 node->links[0] = position.node->links[0];
	 node->left = position.node->left;
	 node->right = position.node->right;

	 _Link_type parent = (_Link_type)position.node->getparent();
     if (parent->left == position.node)
		 parent->left = node;
	 else if (parent->right == position.node) // must judge, because parent maybe header
		 parent->right = node;

	 if (node->left)
		node->left->setparent(node);
	 if (node->right)
		node->right->setparent(node);

	 if (header->left == position.node)
		 header->left = node;
	 else if (header->right == position.node)
		 header->right = node;

	 if (header->getparent() == position.node)
		 header->setparent(node), node->setparent(header);

//	 destroy_node(_Link_type(position.node));
     _S_value(position.node).~_Value();
     this->put_node(_Link_type(position.node), old_data_size);

	 position.node = node;
  }
};

template <class _Key, class _Value, class _KeyOfValue,
          class _Compare, class _Alloc, class _Base>
inline bool
operator==(const _Terark_Rb_tree_common<_Key,_Value,_KeyOfValue,_Compare,_Alloc,_Base>& x,
           const _Terark_Rb_tree_common<_Key,_Value,_KeyOfValue,_Compare,_Alloc,_Base>& y)
{
  return x.size() == y.size() &&
	  std::equal(x.begin(), x.end(), y.begin());
}

template <class _Key, class _Value, class _KeyOfValue,
          class _Compare, class _Alloc, class _Base>
inline bool
operator<(const _Terark_Rb_tree_common<_Key,_Value,_KeyOfValue,_Compare,_Alloc,_Base>& x,
          const _Terark_Rb_tree_common<_Key,_Value,_KeyOfValue,_Compare,_Alloc,_Base>& y)
{
  return lexicographical_compare(x.begin(), x.end(),
                                 y.begin(), y.end());
}

#ifdef TERARK_RB_TREE_FUNCTION_TMPL_PARTIAL_ORDER

template <class _Key, class _Value, class _KeyOfValue,
          class _Compare, class _Alloc, class _Base>
inline bool
operator!=(const _Terark_Rb_tree_common<_Key,_Value,_KeyOfValue,_Compare,_Alloc,_Base>& x,
           const _Terark_Rb_tree_common<_Key,_Value,_KeyOfValue,_Compare,_Alloc,_Base>& y) {
  return !(x == y);
}

template <class _Key, class _Value, class _KeyOfValue,
          class _Compare, class _Alloc, class _Base>
inline bool
operator>(const _Terark_Rb_tree_common<_Key,_Value,_KeyOfValue,_Compare,_Alloc,_Base>& x,
          const _Terark_Rb_tree_common<_Key,_Value,_KeyOfValue,_Compare,_Alloc,_Base>& y) {
  return y < x;
}

template <class _Key, class _Value, class _KeyOfValue,
          class _Compare, class _Alloc, class _Base>
inline bool
operator<=(const _Terark_Rb_tree_common<_Key,_Value,_KeyOfValue,_Compare,_Alloc,_Base>& x,
           const _Terark_Rb_tree_common<_Key,_Value,_KeyOfValue,_Compare,_Alloc,_Base>& y) {
  return !(y < x);
}

template <class _Key, class _Value, class _KeyOfValue,
          class _Compare, class _Alloc, class _Base>
inline bool
operator>=(const _Terark_Rb_tree_common<_Key,_Value,_KeyOfValue,_Compare,_Alloc,_Base>& x,
           const _Terark_Rb_tree_common<_Key,_Value,_KeyOfValue,_Compare,_Alloc,_Base>& y) {
  return !(x < y);
}

template <class _Key, class _Value, class _KeyOfValue,
          class _Compare, class _Alloc, class _Base>
inline void
swap(_Terark_Rb_tree_common<_Key,_Value,_KeyOfValue,_Compare,_Alloc,_Base>& x,
     _Terark_Rb_tree_common<_Key,_Value,_KeyOfValue,_Compare,_Alloc,_Base>& y)
{
  x.swap(y);
}

#endif /* TERARK_RB_TREE_FUNCTION_TMPL_PARTIAL_ORDER */

inline int
black_count(_Terark_Rb_tree_node_base* node, _Terark_Rb_tree_node_base* root)
{
  if (node == 0)
    return 0;
  else {
    int bc = node->isblack() ? 1 : 0;
    if (node == root)
      return bc;
    else
      return bc + black_count(node->getparent(), root);
  }
}

template <class _Key, class _Value, class _KeyOfValue,
          class _Compare, class _Alloc, class _Base>
bool _Terark_Rb_tree_common<_Key,_Value,_KeyOfValue,_Compare,_Alloc,_Base>::rb_verify() const
{
  if (node_count == 0 || begin() == end())
    return node_count == 0 && begin() == end() &&
      header->left == header && header->right == header;

  int len = black_count(leftmost(), root());
  for (const_iterator it = begin(); it != end(); ++it) {
    _Link_type x = (_Link_type) it.node;
    _Link_type __L = _S_left(x);
    _Link_type __R = _S_right(x);

    if (x->isRed())
      if ((__L && __L->isRed()) ||
          (__R && __R->isRed()))
        return false;

    if (__L && key_compare(key_of_node(x), key_of_node(__L)))
      return false;
    if (__R && key_compare(key_of_node(__R), key_of_node(x)))
      return false;

    if (!__L && !__R && black_count(x, root()) != len)
      return false;
  }

  if (leftmost() != _Terark_Rb_tree_node_base::_S_minimum(root()))
    return false;
  if (rightmost() != _Terark_Rb_tree_node_base::_S_maximum(root()))
    return false;

  return true;
}

// Class rb_tree is not part of the C++ standard.  It is provided for
// compatibility with the HP STL.

template <class _Key, class _Value, class _KeyOfValue, class _Compare,
          class _Alloc = TERARK_RB_TREE_DEFAULT_ALLOCATOR(_Value) >
struct rb_tree : public _Terark_Rb_tree<_Key, _Value, _KeyOfValue, _Compare, _Alloc>
{
  typedef _Terark_Rb_tree<_Key, _Value, _KeyOfValue, _Compare, _Alloc> _Base;
  typedef typename _Base::allocator_type allocator_type;

  rb_tree(const _Compare& comp = _Compare(),
          const allocator_type& a = allocator_type())
    : _Base(comp, a) {}

  ~rb_tree() {}
};

#if defined(sgi) && !defined(__GNUC) && (_MIPS_SIM != _MIPS_SIM_ABI32)
#pragma reset woff 1375
#endif

//TERARK_RB_TREE_END_NAMESPACE

#endif /* __TERARK_STL_INTERNAL_TREE_H */

// Local Variables:
// mode:C++
// End:

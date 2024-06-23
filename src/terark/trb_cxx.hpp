/* vim: set tabstop=4 : */
#pragma once

#include "c/trb.h"
#include "mpoolxx.hpp"
#include "io/var_int.hpp"
#include "io/DataIO_Exception.hpp"
#include <assert.h>
#include <string.h>

#include <new>
#include <utility>
#include <iterator>
#include <stdexcept>

#ifndef TRB_NO_HAS_TRIVIAL_DESTRUCTOR
#include <boost/type_traits/has_trivial_destructor.hpp>
#include <boost/type_traits/has_trivial_copy.hpp>
#endif
#include <boost/current_function.hpp>

extern "C" {

//------------------------------------------------------------------------
TERARK_DLL_EXPORT
int
trb_compare_tev_std_string( const struct trb_vtab*  vtab,
							const struct trb_tree*  tree,
							const void*  x,
							const void*  y);

TERARK_DLL_EXPORT
int
trb_compare_tev_std_string_nocase(
		const struct trb_vtab*  vtab,
		const struct trb_tree*  tree,
        const void*  x,
        const void*  y);

//------------------------------------------------------------------------
TERARK_DLL_EXPORT
int
trb_compare_tev_std_wstring(const struct trb_vtab*  vtab,
							const struct trb_tree*  tree,
							const void*  x,
							const void*  y);

TERARK_DLL_EXPORT
int
trb_compare_tev_std_wstring_nocase(
		const struct trb_vtab*  vtab,
		const struct trb_tree*  tree,
        const void*  x,
        const void*  y);

//------------------------------------------------------------------------
// for trbstrmap_imp
TERARK_DLL_EXPORT
int
trb_compare_tev_inline_cstr(
					 const struct trb_vtab*  vtab,
					 const struct trb_tree*  tree,
					 const void*  x,
					 const void*  y);
TERARK_DLL_EXPORT
int
trb_compare_tev_inline_cstr_nocase(
					 const struct trb_vtab*  vtab,
					 const struct trb_tree*  tree,
					 const void*  x,
					 const void*  y);

} // extern "C"

namespace terark {

TERARK_DLL_EXPORT
void trbxx_vtab_init(struct trb_vtab* vtab, field_type_t ikey_type, trb_func_compare key_comp);

//TERARK_DLL_EXPORT void trbxx_raise_duplicate_error(const char* func);

template<class T, int DataOffset>
class trb_iterator_base	: public std::iterator<std::bidirectional_iterator_tag, T>
{
    typedef trb_iterator_base my_t;
protected:
    trb_node* node;

public:
    trb_iterator_base(trb_node* node) : node(node) {}

	T* operator->() const { assert(NULL != node); return  (T*)((char*)node + DataOffset); }
    T& operator* () const { assert(NULL != node); return *(T*)((char*)node + DataOffset); }

    friend bool operator==(const my_t x, const my_t y) { return x.node == y.node; }
    friend bool operator!=(const my_t x, const my_t y) { return x.node != y.node; }

	trb_node* get_node() const { return node; }

    operator bool() const { return NULL != node; }
    bool is_null() const { return NULL == node; }
    bool operator!() const { return NULL == node; }
};

template<class T, int DataOffset, int NodeOffset>
class trb_iterator : public trb_iterator_base<T, DataOffset>
{
    typedef trb_iterator_base<T, DataOffset> super;
    using super::node;
public:
	trb_iterator() : trb_iterator_base<T, DataOffset>(0) {}
    trb_iterator(trb_node* node) : trb_iterator_base<T, DataOffset>(node) {}

    trb_iterator& operator++() { assert(NULL != node); node = trb_iter_next(NodeOffset, node); return *this; }
    trb_iterator& operator--() { assert(NULL != node); node = trb_iter_prev(NodeOffset, node); return *this; }

    trb_iterator operator++(int) { assert(NULL != node); trb_iterator tmp(*this); node = trb_iter_next(NodeOffset, node); return tmp; }
    trb_iterator operator--(int) { assert(NULL != node); trb_iterator tmp(*this); node = trb_iter_prev(NodeOffset, node); return tmp; }
};

template<class T, int DataOffset, int NodeOffset>
class trb_rev_iterator : public trb_iterator_base<T, DataOffset>
{
    typedef trb_iterator_base<T, DataOffset> super;
    using super::node;
public:
	trb_rev_iterator() : trb_iterator_base<T, DataOffset>(0) {}
    trb_rev_iterator(trb_node* node) : trb_iterator_base<T, DataOffset>(node) {}

    trb_rev_iterator& operator++() { assert(NULL != node); node = trb_iter_prev(NodeOffset, node); return *this; }
    trb_rev_iterator& operator--() { assert(NULL != node); node = trb_iter_next(NodeOffset, node); return *this; }

    trb_rev_iterator operator++(int) { assert(NULL != node); trb_rev_iterator tmp(*this); node = trb_iter_prev(NodeOffset, node); return tmp; }
    trb_rev_iterator operator--(int) { assert(NULL != node); trb_rev_iterator tmp(*this); node = trb_iter_next(NodeOffset, node); return tmp; }
};

template<class Whole>
class trb_vtab_cxx_base : public trb_vtab
{
	static
	int
	copy_value(const struct trb_vtab*  vtab,
			   const struct trb_tree*  tree,
			   void*  dest,
			   const void*  src)
	{
		new (dest) Whole (*(const Whole*)src);
		return 1; // alway success
	}

	static
	void
	destroy_value(const struct trb_vtab*  vtab,
				  const struct trb_tree*  tree,
				  void*  data)
	{
		((Whole*)(data))->~Whole();
	}
public:
	trb_vtab_cxx_base()
	{
		memset(this, 0, sizeof(*this));

#ifndef TRB_NO_HAS_TRIVIAL_DESTRUCTOR
		if (boost::has_trivial_destructor<Whole>::value)
			this->pf_data_destroy = NULL;
		else
#endif
			this->pf_data_destroy = &trb_vtab_cxx_base::destroy_value;

		this->pf_data_copy = &trb_vtab_cxx_base::copy_value;
	}
};
TERARK_DLL_EXPORT
trb_node* trbxx_checked_alloc(const trb_vtab& vtab, size_t size, const char* msg);

// compare to std::map::iterator/reverse_iterator
// when iterator reach end(), this iter can not --iter
// when reverse_iterator reach rend(), this iter can not --iter

template< class KeyPart, class Whole=KeyPart
		, int KeyOffset = 0
		, trb_func_compare KeyCompare = &terark_trb_compare_less_tag
		, class Alloc = fixed_mpoolxx<Whole>
		, int NodeOffset = 0 ///< reserved for extension
		, int DataOffset = TRB_DEFAULT_DATA_OFFSET ///< reserved for extension
		>
class trbtab
{
	BOOST_STATIC_ASSERT(DataOffset >= NodeOffset + (int)TRB_DEFAULT_DATA_OFFSET);
protected:
    trb_tree m_tree;

	void init()
	{
		m_tree.trb_count = 0;
		m_tree.trb_root = 0;
	}

public:
//	const int NodeSize = DataOffset + sizeof(Whole);
	enum { NodeSize = DataOffset + sizeof(Whole) };
	struct my_vtab : public trb_vtab_cxx_base<Whole>
	{
		alloc_to_mpool_bridge<Alloc, NodeSize> sa;

		my_vtab()
		{
//			const int NodeSize = DataOffset + sizeof(Whole);

			this->node_offset = NodeOffset;
			this->node_size = NodeSize;
			this->data_offset = DataOffset;
			this->data_size = sizeof(Whole);
			this->key_offset = KeyOffset;
			this->key_size = sizeof(KeyPart);
			this->alloc = sa.get_vtab();

			trbxx_vtab_init(this
					, terark_get_field_type((KeyPart*)0)
					, KeyCompare == &terark_trb_compare_less_tag ? NULL : KeyCompare
					);
		}
	};

	// allow app do custom modify, after modified, must call trb_vtab_init(&s_vtab, key_type);
	static my_vtab s_vtab;

	typedef Alloc allocator_type;
	typedef trb_iterator<Whole, DataOffset, NodeOffset>	  iterator;
	typedef trb_rev_iterator<Whole, DataOffset, NodeOffset> reverse_iterator;

	typedef iterator		 const_iterator;
	typedef reverse_iterator const_reverse_iterator;

	typedef KeyPart		key_type;
	typedef Whole		value_type;
	typedef Whole&		reference;
	typedef Whole*		pointer;
	typedef size_t		size_type;
	typedef ptrdiff_t	difference_type;

    explicit trbtab() { init(); }

	template<class InputIter>
    trbtab(InputIter first, InputIter last) {
		init();
		insert(first, last);
	}

	trbtab(const trbtab& y) {
		trb_copy(&s_vtab, &m_tree, &y.m_tree);
	}

	const trbtab& operator=(const trbtab& y) {
		if (terark_likely(this != &y))
			trb_copy(&s_vtab, &m_tree, &y.m_tree);
		return *this;
	}

	void swap(trbtab& y) {
		trb_tree t = y.m_tree;  y.m_tree = m_tree;  m_tree = t;
	}

	~trbtab() { trb_destroy(&s_vtab, &m_tree); }

	size_type size() const { return m_tree.trb_count; }
	bool empty() const { return 0 == m_tree.trb_count; }

	void clear() { trb_destroy(&s_vtab, &m_tree); }

	iterator begin() const { return iterator(trb_iter_first(NodeOffset, &m_tree)); }
	iterator end() const { return iterator((trb_node*)NULL); }

	reverse_iterator const rbegin() { return reverse_iterator(trb_iter_last(NodeOffset, &m_tree)); }
	reverse_iterator const rend() { return reverse_iterator((trb_node*)NULL); }

// 	const_iterator begin() const { return const_iterator(&m_tree); }
// 	const_iterator end() const { return const_iterator((trb_node*)NULL); }
//
// 	const_reverse_iterator rbegin() const { return const_reverse_iterator(trb_iter_last(&m_tree)); }
// 	const_reverse_iterator rend() const { return const_reverse_iterator((trb_node*)NULL); }
//
	const_iterator cbegin() const { return const_iterator(NodeOffset, &m_tree); }
	const_iterator cend() const { return const_iterator((trb_node*)NULL); }

	const_reverse_iterator crbegin() const { return const_reverse_iterator(trb_iter_last(NodeOffset, &m_tree)); }
	const_reverse_iterator crend() const { return const_reverse_iterator((trb_node*)NULL); }

	std::pair<iterator,bool> insert(const Whole& v) {
		int existed;
		trb_node* node = trb_probe(&s_vtab, &m_tree, &v, &existed);
		return std::pair<iterator,bool>(iterator(node), 0 != existed);
	}
	template<class InputIter>
	void insert(InputIter first, InputIter last) {
		int existed;
		for (; first != last; ++first)
			trb_probe(&s_vtab, &m_tree, &*first, &existed);
	}

	iterator find(const KeyPart& key) const {
		return s_vtab.pf_find(&s_vtab, &m_tree, &key);
	}

	iterator lower_bound(const KeyPart& key) const {
		return s_vtab.pf_lower_bound(&s_vtab, &m_tree, &key);
	}

	iterator upper_bound(const KeyPart& key) const {
		return s_vtab.pf_upper_bound(&s_vtab, &m_tree, &key);
	}

	std::pair<iterator,iterator> equal_range(const KeyPart& key) const {
		trb_node *low, *upp;
		low = s_vtab.pf_equal_range(&s_vtab, &m_tree, &key, &upp);
		return std::pair<iterator,iterator>(low, upp);
	}

	// return erased element count
	size_type erase(const KeyPart& key) {
		return trb_erase(&s_vtab, &m_tree, &key);
	}

	// performance is lower than std::map.erase/std::set.erase
	// because trb_tree need search iterator's path, this is slow
	void erase(iterator iter) {
		assert(!iter.is_null());
		const KeyPart& key = *(const KeyPart*)(s_vtab.key_offset + (char*)&*iter);
		trb_erase(&s_vtab, &m_tree, &key);
	}

	// rarely used:
	static int compare(const KeyPart& x, const KeyPart& y) {
		return s_vtab.pf_compare(&s_vtab, &x, &y);
	}
	struct trb_tree* get_trb_tree() { return &m_tree; }
	const struct trb_tree* get_trb_tree() const { return &m_tree; }

	static Whole* get_data(struct trb_node* node) {
	   	return (Whole*)(DataOffset + (char*)node);
   	}
	static trb_node* alloc_node(size_t size) {
		assert(size >= NodeSize);
	   	return (trb_node*)s_vtab.alloc->salloc(s_vtab.alloc, size);
   	}
	static void free_node(trb_node* node, size_t size) {
		assert(size >= NodeSize);
	   	return s_vtab.alloc->sfree(s_vtab.alloc, node, size);
   	}
	static trb_node* alloc_node() {
	   	return (trb_node*)s_vtab.alloc->salloc(s_vtab.alloc, NodeSize);
   	}
	static void free_node(trb_node* node) {
	   	return s_vtab.alloc->sfree(s_vtab.alloc, node, NodeSize);
   	}
	trb_node* probe_node(trb_node* node) {
		return trb_probe_node(&s_vtab, &m_tree, node);
	}

	template<class DataIO> void dio_load(DataIO& dio) {
		var_size_t size; dio >> size;
		for (size_t c = size.t; c; --c) {
			trb_node *n0, *n1;
			n0 = (trb_node*)trbxx_checked_alloc(s_vtab, NodeSize, BOOST_CURRENT_FUNCTION);
			n1 = trb_probe_node(&s_vtab, &this->m_tree, n0);
			if (terark_unlikely(n1 != n0)) {
			//	trbxx_raise_duplicate_error(n0, NodeSize, BOOST_CURRENT_FUNCTION);
				s_vtab.alloc->sfree(s_vtab.alloc, n0, NodeSize);
				throw DataFormatException("duplicate key");
			}
		}
	}

	template<class DataIO> void dio_save(DataIO& dio) const {
		dio << var_size_t(this->size());
		for (iterator iter = begin(); iter; ++iter)	dio << *iter;
	}
//#ifdef DATA_IO_REG_LOAD_SAVE
//	DATA_IO_REG_LOAD_SAVE(trbtab)
//#endif
  template<class D> friend void DataIO_loadObject(D& d,       trbtab& x) { x.dio_load(d); }
  template<class D> friend void DataIO_saveObject(D& d, const trbtab& x) { x.dio_save(d); }
};

template<class KeyPart, class Whole, int KeyOffset, trb_func_compare KeyCompare, class Alloc, int NodeOffset, int DataOffset>
typename
trbtab<KeyPart, Whole, KeyOffset, KeyCompare, Alloc, NodeOffset, DataOffset>::my_vtab
trbtab<KeyPart, Whole, KeyOffset, KeyCompare, Alloc, NodeOffset, DataOffset>::s_vtab;

template<class Key
		, trb_func_compare KeyCompare = &terark_trb_compare_less_tag
		, class Alloc = fixed_mpoolxx<long> // long is dummy
		, int NodeOffset = 0
		, int DataOffset = TRB_DEFAULT_DATA_OFFSET
		>
class trbset : public trbtab<Key, Key, 0, KeyCompare, Alloc, NodeOffset, DataOffset>
{
	typedef trbtab<Key, Key, 0, KeyCompare, Alloc, NodeOffset, DataOffset> super;
public:
	template<class InputIter>
    trbset(InputIter first, InputIter last) : super(first, last) { }

	trbset() {}

//#ifdef DATA_IO_REG_LOAD_SAVE
//	DATA_IO_REG_LOAD_SAVE(trbset)
//#endif
  template<class D> friend void DataIO_loadObject(D& d,       trbset& x) { x.dio_load(d); }
  template<class D> friend void DataIO_saveObject(D& d, const trbset& x) { x.dio_save(d); }
};

//////////////////////////////////////////////////////////////////////////

template< class Key, class Data
		, trb_func_compare KeyCompare = &terark_trb_compare_less_tag
		, class Alloc = fixed_mpoolxx<long> // long is dummy
		, int NodeOffset = 0
		, int DataOffset = TRB_DEFAULT_DATA_OFFSET
		>
class trbmap
   : public trbtab<const Key, std::pair<const Key, Data>, 0, KeyCompare, Alloc, NodeOffset, DataOffset>
{
	typedef trbtab<const Key, std::pair<const Key, Data>, 0, KeyCompare, Alloc, NodeOffset, DataOffset> super;
public:
	typedef std::pair<const Key, Data> value_type;
	typedef Data mapped_type;

	template<class InputIter>
    trbmap(InputIter first, InputIter last) : super(first, last) { }

	trbmap() {}

	Data& operator[](const Key& key) {
		int existed;
		value_type v(key, Data());
		trb_node* node = trb_probe(
			&super::s_vtab,
		//	&super::m_tree, // vc 2008+sp1 failed on this line
			&this->m_tree,
			&v,
			&existed
			);
		return ((value_type*)((char*)node + DataOffset))->second;
	}
//#ifdef DATA_IO_REG_LOAD_SAVE
//	DATA_IO_REG_LOAD_SAVE(trbmap)
//#endif
  template<class D> friend void DataIO_loadObject(D& d,       trbmap& x) { x.dio_load(d); }
  template<class D> friend void DataIO_saveObject(D& d, const trbmap& x) { x.dio_save(d); }
};

/**
  Key is var-cstr, embeded in the node as inline, not through a pointer to another place;
  Data is embeded too, so in most case, all data is packed in one mem block.

  node layout: [ node_ptrs + Data + klen:byte + key_content(include '\0' end char) ]
    klen=strlen(key_content)+1, assume there is no '\0' in the middle of key_content
	KeyOffset=sizeof(Data)+1, for compare function to get address of key content.
	  so KeyOffset > sizeof(Data), this is invalid for trbtab and most usage of trb_tree!
 */
template< class Data
		, class KeyLenType
		, trb_func_compare KeyCompare = &terark_trb_compare_less_tag
		, class Alloc = mpoolxx<long> // long is dummy
		, int NodeOffset = 0
		, int DataOffset = TRB_DEFAULT_DATA_OFFSET
		>
class trbstrmap_imp
{
	BOOST_STATIC_ASSERT(DataOffset >= NodeOffset + (int)TRB_DEFAULT_DATA_OFFSET);
public:
	enum { keylenlen = sizeof(KeyLenType),  keylenmax = (1L << 8 * sizeof(KeyLenType)) - 1 };
protected:
    trb_tree m_tree;

	static void check_length(size_t keylen) {
		if (terark_unlikely(keylen > keylenmax)) {
			throw std::invalid_argument("keylen too large");
		}
//		assert(keylen > 0);
	}

	static int
	deep_data_copy(const struct trb_vtab*  vtab,
				   const struct trb_tree*  tree,
				         void*  dest,
				   const void*  src)
	{
		new (dest) Data (*reinterpret_cast<const Data*>(src));
		size_t klen = *(const KeyLenType*)((char*)src + sizeof(Data));
		memcpy(dest, (char*)src + sizeof(Data), klen + keylenlen);
		return 1;
	}

	static int
	shallow_data_copy(const struct trb_vtab*  vtab,
					  const struct trb_tree*  tree,
							void*  dest,
					  const void*  src)
	{
		size_t klen = *(const KeyLenType*)((char*)src + sizeof(Data));
		memcpy(dest, src, sizeof(Data) + klen + keylenlen);
		return 1;
	}

public:
	// allow app do custom modify, after modified, must call trb_vtab_init(&s_vtab, key_type);
	struct my_vtab : public trb_vtab_cxx_base<Data>
	{
		alloc_to_mpool_bridge<Alloc, 1> sa;

		my_vtab() {
			this->node_offset = NodeOffset;
			this->node_size = 0;
			this->data_offset = DataOffset;
			this->data_size = sizeof(Data);
			this->key_offset = sizeof(Data) + keylenlen;
			this->key_size = keylenlen;

#ifndef TRB_NO_HAS_TRIVIAL_DESTRUCTOR
		if (boost::has_trivial_copy<Data>::value)
			this->pf_data_copy = shallow_data_copy;
		else
#endif
			this->pf_data_copy = deep_data_copy;

			this->alloc = sa.get_vtab();

			trbxx_vtab_init(this
					, terark_get_field_type((terark_inline_cstr_tag*)0)
					, KeyCompare == &terark_trb_compare_less_tag ? NULL : KeyCompare
					);
		}
	};
	static my_vtab s_vtab;

	typedef Alloc allocator_type;
	typedef trb_iterator<Data, DataOffset, NodeOffset>	  iterator;
	typedef trb_rev_iterator<Data, DataOffset, NodeOffset> reverse_iterator;

	typedef iterator		 const_iterator;
	typedef reverse_iterator const_reverse_iterator;

	typedef const char*	key_type;
	typedef Data		mapped_type;
	typedef Data		value_type;
	typedef Data&		reference;
	typedef Data*		pointer;
	typedef size_t		size_type;
	typedef ptrdiff_t	difference_type;

	void swap(trbstrmap_imp& y) {
		trb_tree t = y.m_tree;  y.m_tree = m_tree;  m_tree = t;
	}

	trbstrmap_imp() { m_tree.trb_count = 0; m_tree.trb_root = 0; }

	trbstrmap_imp(const trbstrmap_imp& y)
	{ // do not use ::trb_copy(...), maybe slower a little..
	  // because vtab->pf_copy is not usable in this way!
/*
		m_tree.trb_count = 0; m_tree.trb_root = 0;
		for (iterator i = y.begin(); i; ++i)
		{
			size_t klen = y.klen(i);
			size_t node_size = DataOffset + sizeof(Data) + klen + keylenlen;
			char* n0 = (char*)s_vtab.alloc->salloc(s_vtab.alloc, node_size);
			new(n0 + DataOffset)Data(*i);
			memcpy(n0 + DataOffset + sizeof(Data), (char*)i.get_node() + DataOffset + sizeof(Data), klen + keylenlen);
			trb_node* n1 = (trb_node*)trb_probe_node(&s_vtab, &this->m_tree, (trb_node*)n0);
			assert(n0 == (char*)n1);
		}
 */
		trb_copy(&s_vtab, &this->m_tree, &y.m_tree);
	}

	const trbstrmap_imp& operator=(const trbstrmap_imp& y) {
		if (terark_likely(this != &y))
			trbstrmap_imp(y).swap(*this);
		return *this;
	}

	~trbstrmap_imp() { trb_destroy(&s_vtab, &m_tree); }

	size_type size() const { return m_tree.trb_count; }
	bool empty() const { return 0 == m_tree.trb_count; }

	void clear() { trb_destroy(&s_vtab, &m_tree); }

	iterator begin() const { return iterator(trb_iter_first(NodeOffset, &m_tree)); }
	iterator end() const { return iterator((trb_node*)NULL); }

	reverse_iterator const rbegin() { return reverse_iterator(trb_iter_last(NodeOffset, &m_tree)); }
	reverse_iterator const rend() { return reverse_iterator((trb_node*)NULL); }

	const_iterator cbegin() const { return const_iterator(NodeOffset, &m_tree); }
	const_iterator cend() const { return const_iterator((trb_node*)NULL); }

	const_reverse_iterator crbegin() const { return const_reverse_iterator(trb_iter_last(NodeOffset, &m_tree)); }
	const_reverse_iterator crend() const { return const_reverse_iterator((trb_node*)NULL); }

//---------------------------------------------------------------------------
	/// @{ parameter is const char*
	iterator find(const key_type key) const {
		return s_vtab.pf_find(&s_vtab, &m_tree, key);
	}

	iterator lower_bound(const key_type key) const {
		return s_vtab.pf_lower_bound(&s_vtab, &m_tree, key);
	}

	iterator upper_bound(const key_type key) const {
		return s_vtab.pf_upper_bound(&s_vtab, &m_tree, key);
	}

	std::pair<iterator,iterator> equal_range(const key_type key) const {
		trb_node *low, *upp;
		low = s_vtab.pf_equal_range(&s_vtab, &m_tree, key, &upp);
		return std::pair<iterator,iterator>(low, upp);
	}

	// return erased element count
	size_type erase(const key_type key) {
		return trb_erase(&s_vtab, &m_tree, key);
	}
	//@}
//---------------------------------------------------------------------------
	///@{ parameter is std::string, std::string is regarded as cstr
	iterator find(const std::string& key) const {
		return s_vtab.pf_find(&s_vtab, &m_tree, key.c_str());
	}

	iterator lower_bound(const std::string& key) const {
		return s_vtab.pf_lower_bound(&s_vtab, &m_tree, key.c_str());
	}

	iterator upper_bound(const std::string& key) const {
		return s_vtab.pf_upper_bound(&s_vtab, &m_tree, key.c_str());
	}

	std::pair<iterator,iterator> equal_range(const std::string& key) const {
		trb_node *low, *upp;
		low = s_vtab.pf_equal_range(&s_vtab, &m_tree, key.c_str(), &upp);
		return std::pair<iterator,iterator>(low, upp);
	}

	// return erased element count
	size_type erase(const std::string& key) {
		return trb_erase(&s_vtab, &m_tree, key.c_str());
	}
	//@}
//***********************************************************************************
	///@{ pack key and search, used for char array, maybe not ended with '\0'

#define TRB_STR_MAP_GEN_FIND(ret_type, fun) \
		check_length(length);	\
		KeyLenType* packed = (KeyLenType*)trbxx_checked_alloc(s_vtab, length+keylenlen, "trbstrmap_imp::"#fun);	\
		packed[0] = length;	\
		memcpy(packed+1, key, length);	\
		ret_type ret = s_vtab.pf_##fun(&s_vtab, &m_tree, packed+1);	\
		s_vtab.alloc->sfree(s_vtab.alloc, packed, length+keylenlen); \
		return ret
//***********************************************************************************
	iterator find(const key_type key, size_t length) const {
		TRB_STR_MAP_GEN_FIND(trb_node*, find);
	}
	iterator lower_bound(const key_type key, size_t length) const {
		TRB_STR_MAP_GEN_FIND(trb_node*, lower_bound);
	}

	iterator upper_bound(const key_type key, size_t length) const {
		TRB_STR_MAP_GEN_FIND(trb_node*, upper_bound);
	}

	std::pair<iterator,iterator> equal_range(const key_type key, size_t length) const {
		check_length(length);
		KeyLenType* packed = (KeyLenType*)trbxx_checked_alloc(s_vtab, length+keylenlen, "trbstrmap_imp::equal_range");
		packed[0] = length;
		memcpy(packed+1, key, length);
		trb_node *low, *upp;
		low = s_vtab.pf_equal_range(&s_vtab, &m_tree, packed+1, &upp);
		s_vtab.alloc->sfree(s_vtab.alloc, packed, length+keylenlen);
		return std::pair<iterator,iterator>(low, upp);
	}

	// return erased element count
	size_type erase(const key_type key, size_t length) {
		TRB_STR_MAP_GEN_FIND(size_type, erase);
	}
	//@}
//***********************************************************************************

	// performance is lower than std::map.erase/std::set.erase
	// because trb_tree need search iterator's path, this is slow
	void erase(iterator iter) {
		assert(!iter.is_null());
		const key_type key = *(const key_type*)(s_vtab.key_offset + (char*)&*iter);
		trb_erase(&s_vtab, &m_tree, key);
	}

	// rarely used:
	static int compare(const key_type x, const key_type y) {
		return s_vtab.pf_compare(&s_vtab, &x, &y);
	}
	struct trb_tree* get_trb_tree() { return &m_tree; }
	const struct trb_tree* get_trb_tree() const { return &m_tree; }

	Data& operator[](const char* key) {
		return probe(key, strlen(key) + 1);
	}

	/// std::string inserted as cstr, not byte array
	Data& operator[](const std::string& key) {
		return probe(key.c_str(), key.size() + 1);
	}

	///@{ hide trbtab::insert intentionally
	///
	std::pair<iterator, bool> insert(const char* key, const Data& val) {
		return insert(key, strlen(key) + 1, val);
	}

	/// std::string inserted as cstr, not byte array
	std::pair<iterator, bool> insert(const std::string& key, const Data& val) {
		return insert(key.c_str(), key.size() + 1, val);
	}

	std::pair<iterator, bool> insert(const std::pair<const char*, Data>& kv) {
		return insert(kv.first, strlen(kv.first) + 1, kv.second);
	}

	/// std::string inserted as cstr, not byte array
	std::pair<iterator, bool> insert(const std::pair<std::string, Data>& kv) {
		return insert(kv.first.c_str(), kv.first.size() + 1, kv.second);
	}
	//@}

	/// find or insert
	Data& probe(const char* key, size_t keylen) {
		std::pair<iterator, bool>
			ib = probe_raw(key, keylen, "trbstrmap_imp::probe");
		if (ib.second) // default construct on raw mem
			new(&*ib.first)Data();
		return *ib.first;
	}

	std::pair<iterator, bool>
		insert(const char* key, size_t keylen, const Data& val)
	{
		std::pair<iterator, bool>
			ib = probe_raw(key, keylen, "trbstrmap_imp::insert");
		if (ib.second) // copy construct on raw mem
			new(&*ib.first)Data(val);
		return ib;
	}

	// do not call constructor on Value's memory
	inline
	std::pair<iterator, bool>
		probe_raw(const char* key, size_t keylen, const char* func)
	{
		check_length(keylen);
		size_t data_size = sizeof(Data) + keylenlen + keylen;
		trb_node *n0, *n1;
		n0 = (trb_node*)trbxx_checked_alloc(s_vtab, DataOffset + data_size, func);
		*(KeyLenType*)((char*)n0 + DataOffset + sizeof(Data)) = keylen;
		memcpy((char*)n0 + DataOffset + sizeof(Data) + keylenlen, key, keylen);
		n1 = trb_probe_node(&s_vtab, &this->m_tree, n0);
		if (n1 == n0) {
			return std::pair<iterator, bool>(n1, true);
		} else {
			s_vtab.alloc->sfree(s_vtab.alloc, n0, DataOffset + data_size);
			return std::pair<iterator, bool>(n1, false);
		}
	}

	/// @note
	/// -# length of key_content is \b klen(iter)+1 \b
	///    \b klen(iter) \b does not include the \b end '\0' \b char
	///
	/// -# \b caution \b if inserted some non-'\0' ended data by insert(key, klen, val)
	///
	static size_t klen(iterator iter) {
		return *(KeyLenType*)((char*)iter.get_node() + DataOffset + sizeof(Data));
	}
	//! @return klen(iter) - 1
	static size_t slen(iterator iter) {
		size_t len = *(KeyLenType*)((char*)iter.get_node() + DataOffset + sizeof(Data)) - 1;
		assert(0 == key(iter)[len]);
		return len;
	}
	static const char* key(iterator iter) {
		return (char*)iter.get_node() + DataOffset + sizeof(Data) + keylenlen;
	}

	static size_t klen(reverse_iterator iter) {
		return *(KeyLenType*)((char*)iter.get_node() + DataOffset + sizeof(Data));
	}
	//! @return klen(iter) - 1
	static size_t slen(reverse_iterator iter) {
		size_t len = *(KeyLenType*)((char*)iter.get_node() + DataOffset + sizeof(Data)) - 1;
		assert(0 == key(iter)[len]);
		return len;
	}
	static const char* key(reverse_iterator iter) {
		return (char*)iter.get_node() + DataOffset + sizeof(Data) + keylenlen;
	}

	//! @param d must be got from `this`
	static size_t klen(const Data* d) {
		return *(const KeyLenType*)((const char*)(d + 1));
	}
	//! @return strlen of key, == klen(d)
	static size_t slen(const Data* d) {
		size_t len = *(const KeyLenType*)((const char*)(d + 1)) - 1;
		assert(0 == key(d)[len]);
		return len;
	}
	static const char* key(const Data* d) {
		return (const char*)(d + 1) + keylenlen;
	}

	//! @{
	//! not compatible with std::map<string, Data>
	//! std::map<string, Data> has no '\0' follow string
	//! but `this` has '\0', except string in `this` has no '\0'
	template<class DataIO> void dio_load(DataIO& dio) {
		var_size_t size; dio >> size;
		for (size_t c = size.t; c; --c) {
			var_size_t klen; dio >> klen;
			check_length(klen.t);
			size_t data_size = sizeof(Data) + keylenlen + klen.t;
			trb_node *n0, *n1;
			n0 = (trb_node*)trbxx_checked_alloc(s_vtab, DataOffset + data_size, BOOST_CURRENT_FUNCTION);
			*(KeyLenType*)((char*)n0 + DataOffset + sizeof(Data)) = klen.t;
			dio.ensureRead((char*)n0 + DataOffset + sizeof(Data) + keylenlen, klen.t);
			dio>> *(Data*)((char*)n0 + DataOffset);
			n1 = trb_probe_node(&s_vtab, &this->m_tree, n0);
			if (n1 != n0) {
			//	trbxx_raise_duplicate_error(n0, DataOffset + data_size, BOOST_CURRENT_FUNCTION);
				s_vtab.alloc->sfree(s_vtab.alloc, n0, DataOffset + data_size);
				throw DataFormatException("duplicate key");
			}
		}
	}

	template<class DataIO> void dio_save(DataIO& dio) const {
		dio << var_size_t(this->size());
		for (iterator iter = begin(); iter; ++iter) {
			dio << var_size_t(klen(iter));
			dio.ensureWrite(key(iter), klen(iter));
			dio << *iter;
		}
	}

	// @}

//#ifdef DATA_IO_REG_LOAD_SAVE
//	DATA_IO_REG_LOAD_SAVE(trbstrmap_imp)
//#endif
  template<class D> friend void DataIO_loadObject(D& d,       trbstrmap_imp& x) { x.dio_load(d); }
  template<class D> friend void DataIO_saveObject(D& d, const trbstrmap_imp& x) { x.dio_save(d); }
};

template<class Data, class KeyLenType, trb_func_compare KeyCompare, class Alloc, int NodeOffset, int DataOffset>
typename
trbstrmap_imp<Data, KeyLenType, KeyCompare, Alloc, NodeOffset, DataOffset>::my_vtab
trbstrmap_imp<Data, KeyLenType, KeyCompare, Alloc, NodeOffset, DataOffset>::s_vtab;

template< class Data
		, trb_func_compare KeyCompare = &terark_trb_compare_less_tag
		, class Alloc = mpoolxx<long> // long is dummy
		, int NodeOffset = 0
		, int DataOffset = TRB_DEFAULT_DATA_OFFSET
		>
class trbstrmap : public trbstrmap_imp<Data, unsigned char, KeyCompare, Alloc, NodeOffset, DataOffset>
{
//#ifdef DATA_IO_REG_LOAD_SAVE
//	DATA_IO_REG_LOAD_SAVE(trbstrmap)
//#endif
  template<class D> friend void DataIO_loadObject(D& d,       trbstrmap& x) { x.dio_load(d); }
  template<class D> friend void DataIO_saveObject(D& d, const trbstrmap& x) { x.dio_save(d); }
};

template< class Data
		, trb_func_compare KeyCompare = &terark_trb_compare_less_tag
		, class Alloc = mpoolxx<long> // long is dummy
		, int NodeOffset = 0
		, int DataOffset = TRB_DEFAULT_DATA_OFFSET
		>
class trbstrmap2 : public trbstrmap_imp<Data, unsigned short, KeyCompare, Alloc, NodeOffset, DataOffset>
{
//#ifdef DATA_IO_REG_LOAD_SAVE
//	DATA_IO_REG_LOAD_SAVE(trbstrmap2)
//#endif
  template<class D> friend void DataIO_loadObject(D& d,       trbstrmap2& x) { x.dio_load(d); }
  template<class D> friend void DataIO_saveObject(D& d, const trbstrmap2& x) { x.dio_save(d); }
};

template< class Data
		, trb_func_compare KeyCompare = &terark_trb_compare_less_tag
		, class Alloc = mpoolxx<long> // long is dummy
		, int NodeOffset = 0
		, int DataOffset = TRB_DEFAULT_DATA_OFFSET
		>
class trbstrmap4 : public trbstrmap_imp<Data, int, KeyCompare, Alloc, NodeOffset, DataOffset>
{
//#ifdef DATA_IO_REG_LOAD_SAVE
//	DATA_IO_REG_LOAD_SAVE(trbstrmap4)
//#endif
  template<class D> friend void DataIO_loadObject(D& d,       trbstrmap4& x) { x.dio_load(d); }
  template<class D> friend void DataIO_saveObject(D& d, const trbstrmap4& x) { x.dio_save(d); }
};

} // name space terark

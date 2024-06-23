/* vim: set tabstop=4 : */
#ifndef __terark_bdb_kmapdset_h__
#define __terark_bdb_kmapdset_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#include <db_cxx.h>
#include "native_compare.hpp"
#include <terark/io/DataIO.hpp>
#include <terark/io/MemStream.hpp>
#include <terark/util/refcount.hpp>
#include <terark/num_to_str.hpp>

#if DB_VERSION_MAJOR > 4 || (DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR >= 6)
#else
#error BerkelyDB version too low, please update
#endif

namespace terark {

class TERARK_DLL_EXPORT kmapdset_iterator_impl_base : public RefCounter
{
public:
	class kmapdset_base* m_owner;
	DBC* m_curp;
	int  m_ret;
	DBT  m_bulk;

public:
	kmapdset_iterator_impl_base(class kmapdset_base* owner);
	void init(DB* dbp, DB_TXN* txn, const char* func);

	virtual ~kmapdset_iterator_impl_base();

	virtual void clear_vec() = 0;
	virtual void push_back(void* data, size_t size) = 0;
	virtual void load_key1(void* data, size_t size) = 0;
	virtual void save_key1(PortableDataOutput<AutoGrownMemIO>& oKey) = 0;

	bool next_key(size_t* cnt, const char* func);
	void bulk_load(DBT* tk1);
	void increment(const char* func);
	void decrement(const char* func);

	bool find_pos(const void* k1, const void* k2, bool bulk, const char* func);

	bool insert(const void* d, const char* func);
	bool update(const void* d, const char* func);
	bool replace(const void* d, const char* func);

	bool remove(const void* k2, const char* func);
	bool remove(const char* func);
};

class TERARK_DLL_EXPORT kmapdset_base
{
	DECLARE_NONE_COPYABLE_CLASS(kmapdset_base)

public:
	DB*    m_db;
	size_t m_bulkSize;
	bt_compare_fcn_type m_bt_comp, m_dup_comp;

	kmapdset_base(DB_ENV* env, const char* dbname
		, u_int32_t flags
		, DB_TXN* txn
		, bt_compare_fcn_type bt_comp
		, bt_compare_fcn_type dup_comp
		, const char* func
		);

	virtual ~kmapdset_base();
	virtual void save_key1(PortableDataOutput<AutoGrownMemIO>& dio, const void* key1) const = 0;
	virtual void save_key2(PortableDataOutput<AutoGrownMemIO>& dio, const void* key2) const = 0;
	virtual void save_data(PortableDataOutput<AutoGrownMemIO>& dio, const void* data) const = 0;

	virtual kmapdset_iterator_impl_base* make_iter() = 0;

	kmapdset_iterator_impl_base* begin_impl(DB_TXN* txn, const char* func);
	kmapdset_iterator_impl_base* end_impl(DB_TXN* txn, const char* func);

	kmapdset_iterator_impl_base* find_impl(const void* k1, DB_TXN* txn, u_int32_t flags, const char* func);
	kmapdset_iterator_impl_base* find_impl(const void* k1, const void* k2, DB_TXN* txn, bool bulk, const char* func);
	kmapdset_iterator_impl_base* upper_bound_impl(const void* k1, DB_TXN* txn, const char* func);
	size_t count_impl(const void* k1, DB_TXN* txn, const char* func);

	bool insert_impl(const void* k1, const void* d, DB_TXN* txn, const char* func);
	bool replace_impl(const void* k1, const void* d, DB_TXN* txn, const char* func);
	bool remove_impl(const void* k1, const void* k2, DB_TXN* txn, const char* func);
	bool remove_impl(const void* k1, DB_TXN* txn, const char* func);
	void clear_impl(DB_TXN* txn, const char* func);
	void flush_impl(const char* func);
};

template<class Data>
struct kmapdset_select_key2
{
	typedef typename Data::key_type type;
};
template<class Key2, class NonKeyData>
struct kmapdset_select_key2<std::pair<Key2, NonKeyData> >
{
	typedef Key2 type;
};

template<class Key1, class Key2, class Data, class Value, class Impl>
class kmapdset_iterator :
	public std::iterator<std::bidirectional_iterator_tag, Value, ptrdiff_t, const Value*, const Value&>
{
	boost::intrusive_ptr<kmapdset_iterator_impl_base> m_impl;

	void copy_on_write()
	{
		if (m_impl->get_refcount() > 1)
		{
			Impl* p = new Impl(m_impl->m_owner);
			m_impl->m_ret = m_impl->m_curp->dup(m_impl->m_curp, &p->m_curp, DB_POSITION);
			TERARK_RT_assert(0 == m_impl->m_ret, std::runtime_error);
			m_impl.reset(p);
		}
	}
private:
#ifdef _MSC_VER
//# pragma warning(disable: 4661) // declaration but not definition
//! MSVC will warning C4661 "declaration but not definition"
void operator++(int) { assert(0); }
void operator--(int) { assert(0); }
#else
//! disable, because clone iterator will cause very much time and resource
void operator++(int);// { assert(0); }
void operator--(int);// { assert(0); }
#endif

public:
	kmapdset_iterator() {}
	explicit kmapdset_iterator(kmapdset_iterator_impl_base* impl)
		: m_impl(impl)
	{
		assert(impl);
		assert(dynamic_cast<Impl*>(impl));
	}

//	bool exist() const { return DB_NOTFOUND != m_impl->m_ret && DB_KEYEMPTY != m_impl->m_ret; }
	bool exist() const { return 0 == m_impl->m_ret; }

	// increment and get key/data-cnt
	bool next_key(size_t& cnt) { return m_impl->next_key(&cnt,BOOST_CURRENT_FUNCTION); }

	bool insert(const Data& d) { return m_impl->insert(&d,BOOST_CURRENT_FUNCTION); }
	bool update(const Data& d) { return m_impl->update(&d,BOOST_CURRENT_FUNCTION); }
	bool replace(const Data& d) { return m_impl->replace(&d,BOOST_CURRENT_FUNCTION); }

	bool remove() const { return m_impl->remove(BOOST_CURRENT_FUNCTION); }
	bool remove(const Key2& k2) const { return m_impl->remove(&k2,BOOST_CURRENT_FUNCTION); }

	kmapdset_iterator& operator++()
	{
		assert(0 == m_impl->m_ret);
		copy_on_write();
		m_impl->increment(BOOST_CURRENT_FUNCTION);
		return *this;
	}
	kmapdset_iterator& operator--()
	{
		assert(0 == m_impl->m_ret);
		copy_on_write();
		m_impl->decrement(BOOST_CURRENT_FUNCTION);
		return *this;
	}
	const Value& operator*() const
	{
		assert(0 == m_impl->m_ret);
		return static_cast<Impl*>(m_impl.get())->m_kdv;
	}
	const Value* operator->() const
	{
		assert(0 == m_impl->m_ret);
		return &static_cast<Impl*>(m_impl.get())->m_kdv;
	}
	Value& get_mutable() const
	{
		assert(0 == m_impl->m_ret);
		return static_cast<Impl*>(m_impl.get())->m_kdv;
	}
};

template<class Key1, class Data>
class kmapdset : protected kmapdset_base
{
	DECLARE_NONE_COPYABLE_CLASS(kmapdset)

public:
	typedef Key1
			key1_t, key_type;
	typedef typename kmapdset_select_key2<Data>::type
			key2_t;
	typedef Data
			data_type, data_t;
	typedef std::pair<Key1, std::vector<Data> >
			value_type;
	typedef std::vector<Data>
			data_vec_t;
	typedef typename std::vector<Data>::const_iterator
			data_iter_t;

protected:
	class kmapdset_iterator_impl : public kmapdset_iterator_impl_base
	{
	public:
		value_type m_kdv;

		kmapdset_iterator_impl(kmapdset_base* owner)
			: kmapdset_iterator_impl_base(owner)
		{}

		virtual void clear_vec()
		{
			m_kdv.second.resize(0);
		}

		virtual void push_back(void* data, size_t size)
		{
			Data x;
			PortableDataInput<MinMemIO> iData;
			iData.set(data);
			iData >> x;
			TERARK_RT_assert(iData.diff(data) == size, std::logic_error);
			m_kdv.second.push_back(x);
		}
		virtual void load_key1(void* data, size_t size)
		{
			PortableDataInput<MemIO> iKey1;
			iKey1.set(data, size);
			iKey1 >> m_kdv.first;
			TERARK_RT_assert(iKey1.diff(data) == size, std::logic_error);
		}
		virtual void save_key1(PortableDataOutput<AutoGrownMemIO>& oKey1)
		{
			oKey1 << m_kdv.first;
		}
	};

	//! overrides
	void save_key1(PortableDataOutput<AutoGrownMemIO>& dio, const void* key1) const { dio << *(const key1_t*)key1; }
	void save_key2(PortableDataOutput<AutoGrownMemIO>& dio, const void* key2) const { dio << *(const key2_t*)key2; }
	void save_data(PortableDataOutput<AutoGrownMemIO>& dio, const void* data) const { dio << *(const data_t*)data; }

	kmapdset_iterator_impl_base* make_iter() { return new kmapdset_iterator_impl(this); }

public:

	typedef kmapdset_iterator<Key1, key2_t, Data, value_type, kmapdset_iterator_impl>
			iterator, const_iterator;

	//! constructor
	//! @param bt_comp, dup_comp
	//!    Key 比较函数，默认情况下自动推导
	//!    -# 如果 Key 是预定义类型，会推导至相应的比较函数
	//!    -# 如果是自定义类型，会推导至 Key::bdb_bt_compare/key2_t::bdb_bt_compare, 如果未定义此函数或原型不匹配，都会报错
	//!    -# 也可以直接指定，从而禁止自动推导
	kmapdset(DB_ENV* env, const char* dbname
		, u_int32_t flags = DB_CREATE
		, DB_TXN* txn = NULL
		, bt_compare_fcn_type bt_comp = bdb_auto_bt_compare((key1_t*)(0))
		, bt_compare_fcn_type dup_comp = bdb_auto_bt_compare((key2_t*)(0))
		)
		: kmapdset_base(env, dbname, flags, txn, bt_comp, dup_comp, BOOST_CURRENT_FUNCTION)
	{
	}
	kmapdset(DbEnv* env, const char* dbname
		, u_int32_t flags = DB_CREATE
		, DbTxn* txn = NULL
		, bt_compare_fcn_type bt_comp = bdb_auto_bt_compare((key1_t*)(0))
		, bt_compare_fcn_type dup_comp = bdb_auto_bt_compare((key2_t*)(0))
		)
		: kmapdset_base(env->get_DB_ENV(), dbname, flags, txn ? txn->get_DB_TXN() : NULL, bt_comp, dup_comp, BOOST_CURRENT_FUNCTION)
	{
	}

	iterator begin(DB_TXN* txn = NULL) { return iterator(begin_impl(txn, BOOST_CURRENT_FUNCTION)); }
	iterator end  (DB_TXN* txn = NULL) { return iterator(end_impl  (txn, BOOST_CURRENT_FUNCTION)); }

	iterator begin(DbTxn* txn) { return iterator(begin_impl(txn->get_DB_TXN(), BOOST_CURRENT_FUNCTION)); }
	iterator end  (DbTxn* txn) { return iterator(end_impl  (txn->get_DB_TXN(), BOOST_CURRENT_FUNCTION)); }

	iterator find(const Key1& k1, DB_TXN* txn = NULL)
	{
		return iterator(find_impl(&k1, txn, DB_SET|DB_MULTIPLE, BOOST_CURRENT_FUNCTION));
	}
	iterator find(const Key1& k1, DbTxn* txn)
	{
		return iterator(find_impl(&k1, txn->get_DB_TXN(), DB_SET|DB_MULTIPLE, BOOST_CURRENT_FUNCTION));
	}

	iterator find(const Key1& k1, const key2_t& k2, DB_TXN* txn = NULL)
	{
		return iterator(find_impl(&k1, &k2, txn, false, BOOST_CURRENT_FUNCTION));
	}
	iterator find(const Key1& k1, const key2_t& k2, DbTxn* txn)
	{
		return iterator(find_impl(&k1, &k2, txn->get_DB_TXN(), false, BOOST_CURRENT_FUNCTION));
	}

	iterator find_md(const Key1& k1, const key2_t& k2, DB_TXN* txn = NULL)
	{
		return iterator(find_impl(&k1, &k2, txn, true, BOOST_CURRENT_FUNCTION));
	}
	iterator find_md(const Key1& k1, const key2_t& k2, DbTxn* txn)
	{
		return iterator(find_impl(&k1, &k2, txn->get_DB_TXN(), true, BOOST_CURRENT_FUNCTION));
	}

	iterator lower_bound(const Key1& k1, DB_TXN* txn = NULL)
	{
		return iterator(find_impl(&k1, txn, DB_SET_RANGE|DB_MULTIPLE, BOOST_CURRENT_FUNCTION));
	}
	iterator lower_bound(const Key1& k1, DbTxn* txn)
	{
		return iterator(find_impl(&k1, txn->get_DB_TXN(), DB_SET_RANGE|DB_MULTIPLE, BOOST_CURRENT_FUNCTION));
	}
	iterator upper_bound(const Key1& k1, DB_TXN* txn = NULL)
	{
		return iterator(upper_bound_impl(&k1, txn, BOOST_CURRENT_FUNCTION));
	}

	bool insert(const Key1& k1, const Data& d, DB_TXN* txn = NULL)
	{
		return insert_impl(&k1, &d, txn, BOOST_CURRENT_FUNCTION);
	}
	bool insert(const Key1& k1, const Data& d, DbTxn* txn)
	{
		return insert_impl(&k1, &d, txn->get_DB_TXN(), BOOST_CURRENT_FUNCTION);
	}

	bool replace(const Key1& k1, const Data& d, DB_TXN* txn = NULL)
	{
		return replace_impl(&k1, &d, txn, BOOST_CURRENT_FUNCTION);
	}
	bool replace(const Key1& k1, const Data& d, DbTxn* txn)
	{
		return replace_impl(&k1, &d, txn->get_DB_TXN(), BOOST_CURRENT_FUNCTION);
	}

	bool remove(const Key1& k1, const key2_t& k2, DB_TXN* txn = NULL)
	{
		return remove_impl(&k1, &k2, txn, BOOST_CURRENT_FUNCTION);
	}
	bool remove(const Key1& k1, const key2_t& k2, DbTxn* txn)
	{
		return remove_impl(&k1, &k2, txn->get_DB_TXN(), BOOST_CURRENT_FUNCTION);
	}

	bool remove(const Key1& k1, DB_TXN* txn = NULL)
	{
		return remove_impl(&k1, txn, BOOST_CURRENT_FUNCTION);
	}
	bool remove(const Key1& k1, DbTxn* txn)
	{
		return remove_impl(&k1, txn->get_DB_TXN(), BOOST_CURRENT_FUNCTION);
	}

	bool erase(const iterator& iter)
	{
		return iter.remove();
	}

	void clear(DB_TXN* txn = NULL)
	{
		clear_impl(txn, BOOST_CURRENT_FUNCTION);
	}
	void clear(DbTxn* txn)
	{
		return clear_impl(txn->get_DB_TXN(), BOOST_CURRENT_FUNCTION);
	}

	void flush()
	{
		return flush_impl(BOOST_CURRENT_FUNCTION);
	}

	size_t count(const Key1& k1, DB_TXN* txn = NULL)
	{
		return count_impl(&k1, txn, BOOST_CURRENT_FUNCTION);
	}
	size_t count(const Key1& k1, DbTxn* txn)
	{
		return count_impl(&k1, txn ? txn->get_DB_TXN() : NULL, BOOST_CURRENT_FUNCTION);
	}

	DB* getDB() { return m_db; }
	const DB* getDB() const { return m_db; }
};

} // namespace terark


#endif // __terark_bdb_kmapdset_h__


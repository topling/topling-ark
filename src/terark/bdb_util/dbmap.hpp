/* vim: set tabstop=4 : */
#ifndef __terark_bdb_dbmap_h__
#define __terark_bdb_dbmap_h__

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

class TERARK_DLL_EXPORT dbmap_iterator_impl_base : public RefCounter
{
public:
	class dbmap_base* m_owner;
	DBC*  m_curp;
	int   m_ret;

public:
	dbmap_iterator_impl_base(class dbmap_base* owner);
	void init(DB* dbp, DB_TXN* txn, const char* func);

	virtual ~dbmap_iterator_impl_base();

	virtual void load_key(void* data, size_t size) = 0;
	virtual void load_val(void* data, size_t size) = 0;
	virtual void save_key(PortableDataOutput<AutoGrownMemIO>& oKey) = 0;

	void advance(u_int32_t direction_flag, const char* func);

	void update(const void* d, const char* func);
	void remove(const char* func);
};

class TERARK_DLL_EXPORT dbmap_base
{
	DECLARE_NONE_COPYABLE_CLASS(dbmap_base)

public:
	DB*    m_db;
	size_t m_bulkSize;
	bt_compare_fcn_type m_bt_comp;

	dbmap_base(DB_ENV* env, const char* dbname
		, u_int32_t flags
		, DB_TXN* txn
		, bt_compare_fcn_type bt_comp
		, const char* func
		);

	virtual ~dbmap_base();
	virtual void save_key(PortableDataOutput<AutoGrownMemIO>& dio, const void* key) const = 0;
	virtual void save_val(PortableDataOutput<AutoGrownMemIO>& dio, const void* data) const = 0;

	virtual dbmap_iterator_impl_base* make_iter() = 0;

	dbmap_iterator_impl_base* begin_impl(DB_TXN* txn, const char* func);
	dbmap_iterator_impl_base* end_impl(DB_TXN* txn, const char* func);

	dbmap_iterator_impl_base* find_impl(const void* k, DB_TXN* txn, u_int32_t flags, const char* func);

	bool insert_impl(const void* k, const void* d, u_int32_t flags, DB_TXN* txn, const char* func);
	bool remove_impl(const void* k, DB_TXN* txn, const char* func);
	void clear_impl(DB_TXN* txn, const char* func);
};

template<class Key, class Val, class Value, class Impl>
class dbmap_iterator :
	public std::iterator<std::bidirectional_iterator_tag, Value, ptrdiff_t, const Value*, const Value&>
{
	boost::intrusive_ptr<dbmap_iterator_impl_base> m_impl;

	void copy_on_write()
	{
		if (m_impl->get_refcount() > 1)
		{
			Impl* q = static_cast<Impl*>(m_impl.get());
			Impl* p = new Impl(q->m_owner);
			q->m_ret = q->m_curp->dup(q->m_curp, &p->m_curp, DB_POSITION);
			TERARK_RT_assert(0 == q->m_ret, std::runtime_error);
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
	dbmap_iterator() {}
	explicit dbmap_iterator(dbmap_iterator_impl_base* impl)
		: m_impl(impl)
	{
		assert(impl);
		assert(dynamic_cast<Impl*>(impl));
	}

//	bool exist() const { return DB_NOTFOUND != m_impl->m_ret && DB_KEYEMPTY != m_impl->m_ret; }
	bool exist() const { return 0 == m_impl->m_ret; }

	void update(const Val& val) { m_impl->update(&val, BOOST_CURRENT_FUNCTION); }

	void remove() { m_impl->remove(BOOST_CURRENT_FUNCTION); }

	dbmap_iterator& operator++()
	{
		assert(0 == m_impl->m_ret);
		copy_on_write();
		m_impl->advance(DB_NEXT, BOOST_CURRENT_FUNCTION);
		return *this;
	}
	dbmap_iterator& operator--()
	{
		assert(0 == m_impl->m_ret);
		copy_on_write();
		m_impl->advance(DB_PREV, BOOST_CURRENT_FUNCTION);
		return *this;
	}
	const Value& operator*() const
	{
		assert(0 == m_impl->m_ret);
		return static_cast<Impl*>(m_impl.get())->m_kv;
	}
	const Value* operator->() const
	{
		assert(0 == m_impl->m_ret);
		return &static_cast<Impl*>(m_impl.get())->m_kv;
	}
	Value& get_mutable() const
	{
		assert(0 == m_impl->m_ret);
		return static_cast<Impl*>(m_impl.get())->m_kv;
	}
};

template<class Key, class Val>
class dbmap : protected dbmap_base
{
	DECLARE_NONE_COPYABLE_CLASS(dbmap)

public:
	typedef Key		key_type;
	typedef std::pair<Key, Val> value_type;

protected:
	class dbmap_iterator_impl : public dbmap_iterator_impl_base
	{
	public:
		value_type m_kv;

		dbmap_iterator_impl(dbmap_base* owner)
			: dbmap_iterator_impl_base(owner)
		{}

		virtual void load_key(void* data, size_t size)
		{
			PortableDataInput<MemIO> iKey;
			iKey.set(data, size);
			iKey >> m_kv.first;
			TERARK_RT_assert(iKey.eof(), std::logic_error);
		}
		virtual void load_val(void* data, size_t size)
		{
			PortableDataInput<MinMemIO> iVal;
			iVal.set(data);
			iVal >> m_kv.second;
			TERARK_RT_assert(iVal.diff(data) == size, std::logic_error);
		}
		virtual void save_key(PortableDataOutput<AutoGrownMemIO>& oKey1)
		{
			oKey1 << m_kv.first;
		}
	};

	//! overrides
	void save_key(PortableDataOutput<AutoGrownMemIO>& dio, const void* key) const { dio << *(const Key*)key; }
	void save_val(PortableDataOutput<AutoGrownMemIO>& dio, const void* val) const { dio << *(const Val*)val; }

	dbmap_iterator_impl_base* make_iter() { return new dbmap_iterator_impl(this); }

public:

	typedef dbmap_iterator<Key, Val, value_type, dbmap_iterator_impl>
			iterator, const_iterator;

	//! constructor
	//! @param bt_comp
	//!    Key 比较函数，默认情况下自动推导
	//!    -# 如果 Key 是预定义类型，会推导至相应的比较函数
	//!    -# 如果是自定义类型，会推导至 Key::bdb_bt_compare, 如果未定义此函数或原型不匹配，都会报错
	//!    -# 也可以直接指定，从而禁止自动推导
	dbmap(DB_ENV* env, const char* dbname
		, u_int32_t flags = DB_CREATE
		, DB_TXN* txn = NULL
		, bt_compare_fcn_type bt_comp = bdb_auto_bt_compare((Key*)(0))
		)
		: dbmap_base(env, dbname, txn, bt_comp, BOOST_CURRENT_FUNCTION)
	{
	}
	dbmap(DbEnv* env, const char* dbname
		, u_int32_t flags = DB_CREATE
		, DbTxn* txn = NULL
		, bt_compare_fcn_type bt_comp = bdb_auto_bt_compare((Key*)(0))
		)
		: dbmap_base(env->get_DB_ENV(), dbname, flags, txn ? txn->get_DB_TXN() : NULL, bt_comp, BOOST_CURRENT_FUNCTION)
	{
	}

	iterator begin(DB_TXN* txn = NULL) { return iterator(begin_impl(txn, BOOST_CURRENT_FUNCTION)); }
	iterator end  (DB_TXN* txn = NULL) { return iterator(end_impl  (txn, BOOST_CURRENT_FUNCTION)); }

	iterator begin(DbTxn* txn) { return iterator(begin_impl(txn ? txn->get_DB_TXN() : NULL, BOOST_CURRENT_FUNCTION)); }
	iterator end  (DbTxn* txn) { return iterator(end_impl  (txn ? txn->get_DB_TXN() : NULL, BOOST_CURRENT_FUNCTION)); }

	value_type back()
	{
		iterator iter = this->end();
		--iter;
		if (iter.exist())
			return *iter;
		throw std::runtime_error(BOOST_CURRENT_FUNCTION);
	}
	value_type front()
	{
		iterator iter = this->begin();
		if (iter.exist())
			return *iter;
		throw std::runtime_error(BOOST_CURRENT_FUNCTION);
	}

	iterator find(const Key& k, DB_TXN* txn = NULL)
	{
		return iterator(find_impl(&k, txn, DB_SET, BOOST_CURRENT_FUNCTION));
	}
	iterator find(const Key& k, DbTxn* txn)
	{
		return iterator(find_impl(&k, txn ? txn->get_DB_TXN() : NULL, DB_SET, BOOST_CURRENT_FUNCTION));
	}

	iterator lower_bound(const Key& k, DB_TXN* txn = NULL)
	{
		return iterator(find_impl(&k, txn, DB_SET_RANGE, BOOST_CURRENT_FUNCTION));
	}
	iterator lower_bound(const Key& k, DbTxn* txn)
	{
		return iterator(find_impl(&k, txn ? txn->get_DB_TXN() : NULL, DB_SET_RANGE, BOOST_CURRENT_FUNCTION));
	}

	bool insert(const std::pair<Key,Val>& kv, DB_TXN* txn = NULL)
	{
		return insert_impl(&kv.first, &kv.second, DB_NOOVERWRITE, txn, BOOST_CURRENT_FUNCTION);
	}
	bool insert(const std::pair<Key,Val>& kv, DbTxn* txn)
	{
		return insert_impl(&kv.first, &kv.second, DB_NOOVERWRITE, txn ? txn->get_DB_TXN() : NULL, BOOST_CURRENT_FUNCTION);
	}

	bool insert(const Key& k, const Val& d, DB_TXN* txn = NULL)
	{
		return insert_impl(&k, &d, DB_NOOVERWRITE, txn, BOOST_CURRENT_FUNCTION);
	}
	bool insert(const Key& k, const Val& d, DbTxn* txn)
	{
		return insert_impl(&k, &d, DB_NOOVERWRITE, txn ? txn->get_DB_TXN() : NULL, BOOST_CURRENT_FUNCTION);
	}

	void replace(const std::pair<Key,Val>& kv, DB_TXN* txn = NULL)
	{
		insert_impl(&kv.first, &kv.second, 0, txn, BOOST_CURRENT_FUNCTION);
	}
	void replace(const std::pair<Key,Val>& kv, DbTxn* txn)
	{
		insert_impl(&kv.first, &kv.second, 0, txn ? txn->get_DB_TXN() : NULL, BOOST_CURRENT_FUNCTION);
	}
	void replace(const Key& k, const Val& d, DB_TXN* txn = NULL)
	{
		insert_impl(&k, &d, 0, txn, BOOST_CURRENT_FUNCTION);
	}
	void replace(const Key& k, const Val& d, DbTxn* txn)
	{
		insert_impl(&k, &d, 0, txn ? txn->get_DB_TXN() : NULL, BOOST_CURRENT_FUNCTION);
	}

	bool remove(const Key& k, DB_TXN* txn = NULL)
	{
		return remove_impl(&k, txn, BOOST_CURRENT_FUNCTION);
	}
	bool remove(const Key& k, DbTxn* txn)
	{
		return remove_impl(&k, txn ? txn->get_DB_TXN() : NULL, BOOST_CURRENT_FUNCTION);
	}

	bool erase(iterator& iter)
	{
		return iter.remove();
	}

	void clear(DB_TXN* txn = NULL)
	{
		clear_impl(txn, BOOST_CURRENT_FUNCTION);
	}
	void clear(DbTxn* txn)
	{
		return clear_impl(txn ? txn->get_DB_TXN() : NULL, BOOST_CURRENT_FUNCTION);
	}

	DB* getDB() { return m_db; }
	const DB* getDB() const { return m_db; }
};

} // namespace terark


#endif // __terark_bdb_dbmap_h__

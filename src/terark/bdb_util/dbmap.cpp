/* vim: set tabstop=4 : */
#include "dbmap.hpp"
#include <terark/num_to_str.hpp>

/**
 @class dbmap 使用DB的键值映射表

这个设计的的基本原则就是：模板作为一个薄的、类型安全的包装层，
实现层的代码可以多个模板实例来公用，这样不但加快了编译时间，也减小了生成的代码尺寸。

这个实现相当于std::map<Key,Val>
但接口上也不完全相同，主要是基于易实现和性能考虑

-# 一般情况下，不需要特别指定 key_compare_function, 这个类可以根据指定的 Key 类型自动推导出相应的 key_compare_function
   @code
    using namespace std;
	using namesapce terark;
	// some code...
	DB_ENV* env;
	// ...
	//
    dbmap<string, int32_t> map(env, "my_db"); // 默认使用 bdb_locale_strcmp_nocase
   @endcode
-# 如果需要指定 key_compare_function，可以在构造函数中指定。
-# 如果 Key 类型是自定义的，可以在 Key 的定义中增加一个 static int bdb_bt_compare(DBT*, DBT*) 函数
   不需要在 dbmap<Key,Val> 构造函数显示指定，它会被自动引用。@see \c dbmap
-# 目前，dbmap::iterator 不能象标准 iterator 一样用 first, last 来使用，结尾判断只能用 iterator.exist()
   @code
    // std::map
	for (dbmap<Key, Val>::iterator iter = m.begin(); iter != m.end(); ++iter)
	{
		pair<Key,Val>& kv = *iter;
	//	do some thing
	}
    // dbmap
	for (dbmap<Key, Val>::iterator iter = dbm.begin(); iter.exist(); ++iter)
	{
		pair<Key,Val>& kv = *iter;
	//	do some thing
	}
   @endcode
-# 另一个不同之处在于使用 iterator 从后往前迭代：
 @code
	for (dbmap<Key, Val>::iterator iter = dbm.end();;)
	{
		--iter;
		if (!iter.exist()) break;

		pair<Key,Val>& kv = *iter;
	//	do some thing
	}
 @endcode
*/


namespace terark {

dbmap_iterator_impl_base::dbmap_iterator_impl_base(class dbmap_base* owner)
	: m_owner(owner)
	, m_curp(0), m_ret(-1)
{
}

void dbmap_iterator_impl_base::init(DB* dbp, DB_TXN* txn, const char* func)
{
	int ret = dbp->cursor(dbp, txn, &m_curp, 0);
	if (0 != ret)
	{
		delete this;
		string_appender<> oss;
		oss << db_strerror(ret) << "... at: " << func;
		throw std::runtime_error(oss.str());
	}
	m_ret = 0;
}

dbmap_iterator_impl_base::~dbmap_iterator_impl_base()
{
	if (m_curp)
		m_curp->close(m_curp);
}

void dbmap_iterator_impl_base::advance(u_int32_t direction_flag, const char* func)
{
	TERARK_RT_assert(0 == m_ret, std::logic_error);
	DBT tk; memset(&tk, 0, sizeof(DBT));
	DBT td; memset(&td, 0, sizeof(DBT));
	m_ret = m_curp->get(m_curp, &tk, &td, direction_flag);
	if (0 == m_ret)
	{
		load_key(tk.data, tk.size);
		load_val(td.data, td.size);
	}
	else if (DB_NOTFOUND != m_ret)
	{
		string_appender<> oss;
		oss << db_strerror(m_ret) << "... at: " << func;
		throw std::runtime_error(oss.str());
	}
}

/**
 @brief
 @return true successful updated
		false (key, d.key2) did not exist, not updated
 @throw other errors
 */
void dbmap_iterator_impl_base::update(const void* d, const char* func)
{
	TERARK_RT_assert(0 == m_ret, std::logic_error);
	PortableDataOutput<AutoGrownMemIO> oKey1, oData;
	this->save_key(oKey1);
	m_owner->save_val(oData, d);
	DBT tk; memset(&tk, 0, sizeof(DBT)); tk.data = oKey1.begin(); tk.size = oKey1.tell();
	DBT td; memset(&td, 0, sizeof(DBT)); td.data = oData.begin(); td.size = oData.tell();
	int ret = m_curp->put(m_curp, &tk, &td, DB_CURRENT);
	if (0 != ret)
	{
		string_appender<> oss;
		oss << db_strerror(ret)
			<< "... at: " << func;
		throw std::runtime_error(oss.str());
	}
}

void dbmap_iterator_impl_base::remove(const char* func)
{
	TERARK_RT_assert(0 == m_ret, std::logic_error);
	int ret = m_curp->del(m_curp, 0);
	if (0 != ret)
	{
		string_appender<> oss;
		oss << db_strerror(ret)
			<< "... at: " << func;
		throw std::runtime_error(oss.str());
	}
}

//////////////////////////////////////////////////////////////////////////

dbmap_base::dbmap_base(DB_ENV* env, const char* dbname
    , u_int32_t flags
	, DB_TXN* txn
	, bt_compare_fcn_type bt_comp
	, const char* func
	)
	: m_bt_comp(0)
{
	m_bulkSize = 512*1024;
	m_db = 0;
	int ret = db_create(&m_db, env, 0);
	if (0 == ret)
	{
		if (bt_comp) {
			m_bt_comp = bt_comp;
			m_db->set_bt_compare(m_db, bt_comp);
		}
		m_db->app_private = (this);
		ret = m_db->open(m_db, txn, dbname, dbname, DB_BTREE, flags, 0);
	}

	if (0 != ret)
	{
		string_appender<> oss;
		oss << db_strerror(ret)
			<< "... at: " << func;
		throw std::runtime_error(oss.str());
	}
}

dbmap_base::~dbmap_base()
{
	if (m_db)
		m_db->close(m_db, 0);
}

dbmap_iterator_impl_base* dbmap_base::begin_impl(DB_TXN* txn, const char* func)
{
	dbmap_iterator_impl_base* iter = make_iter();
	iter->init(m_db, txn, func);
	DBT tk; memset(&tk, 0, sizeof(DBT));
	DBT td; memset(&td, 0, sizeof(DBT));
	iter->m_ret = iter->m_curp->get(iter->m_curp, &tk, &td, DB_FIRST);
	if (0 == iter->m_ret)
	{
		iter->load_key(tk.data, tk.size);
		iter->load_val(td.data, td.size);
	}
	else if (DB_NOTFOUND != iter->m_ret)
	{
		string_appender<> oss;
		oss << db_strerror(iter->m_ret)
			<< "... at: " << func;
		delete iter; iter = 0;
		throw std::runtime_error(oss.str());
	}
	return iter;
}

dbmap_iterator_impl_base* dbmap_base::end_impl(DB_TXN* txn, const char* func)
{
	dbmap_iterator_impl_base* iter = make_iter();
	iter->init(m_db, txn, func);
//	iter->m_ret = DB_NOTFOUND;
	return iter;
}

dbmap_iterator_impl_base* dbmap_base::find_impl(const void* k, DB_TXN* txn, u_int32_t flags, const char* func)
{
	PortableDataOutput<AutoGrownMemIO> oKey1;
	save_key(oKey1, k);
	DBT tk; memset(&tk, 0, sizeof(DBT)); tk.data = oKey1.begin(); tk.size = oKey1.tell();
	DBT td; memset(&td, 0, sizeof(DBT));
	dbmap_iterator_impl_base* iter = make_iter();
	iter->init(m_db, txn, func);
	iter->m_ret = iter->m_curp->get(iter->m_curp, &tk, &td, flags);
	if (0 == iter->m_ret)
	{
		iter->load_key(tk.data, tk.size);
		iter->load_val(td.data, td.size);
	}
	else if (DB_NOTFOUND != iter->m_ret && DB_KEYEMPTY != iter->m_ret)
	{
		string_appender<> oss;
		oss << db_strerror(iter->m_ret)
			<< "... at: " << func
			<< "\n"
			<< "flags=" << flags
			;
		throw std::runtime_error(oss.str());
	}
	return iter;
}

/**
 @brief insert a record
 @return true success,  no same k-k2 in db, the record was inserted
		 false failed, has same k-k2 in db, the record was not inserted, not replaced existing yet
 @throws exception, failed
 */
bool dbmap_base::insert_impl(const void* k, const void* d, u_int32_t flags, DB_TXN* txn, const char* func)
{
	PortableDataOutput<AutoGrownMemIO> oKey1, oData;
	try {
		save_key(oKey1, k);
		save_val(oData, d);
	}
	catch (const IOException& exp)
	{
		string_appender<> oss;
		oss << exp.what() << "... at: " << func;
		throw std::runtime_error(oss.str());
	}
	DBT tk; memset(&tk, 0, sizeof(DBT)); tk.data = oKey1.begin(); tk.size = oKey1.tell();
	DBT td; memset(&td, 0, sizeof(DBT)); td.data = oData.begin(); td.size = oData.tell();
	int ret = m_db->put(m_db, txn, &tk, &td, flags);
	if (DB_KEYEXIST == ret)
		return false;
	if (0 == ret)
		return true;
	string_appender<> oss;
	oss << db_strerror(ret)
		<< "... at: " << func
		<< "\n"
		;
	throw std::runtime_error(oss.str());
}

/**
 @brief
 @return true (k) existed, remove success
        false (k) not existed, nothing done
 */
bool dbmap_base::remove_impl(const void* k, DB_TXN* txn, const char* func)
{
	PortableDataOutput<AutoGrownMemIO> oKey1;
	save_key(oKey1, k);
	DBT tk; memset(&tk, 0, sizeof(DBT)); tk.data = oKey1.begin(); tk.size = oKey1.tell();
	int ret = m_db->del(m_db, txn, &tk, 0);
	if (DB_NOTFOUND == ret)
		return false;
	if (0 == ret)
		return true;
	string_appender<> oss;
	oss << db_strerror(ret)
		<< "... at: " << func
		<< "\n"
		;
	throw std::runtime_error(oss.str());
}

void dbmap_base::clear_impl(DB_TXN* txn, const char* func)
{
	u_int32_t count;
	int ret = m_db->truncate(m_db, txn, &count, 0);
	if (0 != ret)
	{
		string_appender<> oss;
		oss << db_strerror(ret)
			<< "... at: " << func
			<< "\n"
			;
		throw std::runtime_error(oss.str());
	}
}

} // namespace terark


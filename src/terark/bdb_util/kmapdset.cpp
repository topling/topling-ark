/* vim: set tabstop=4 : */
#include "kmapdset.hpp"
#include <terark/num_to_str.hpp>

namespace terark {

/**
 @class kmapdset 使用DB从单个键映射到多个排序的值

这个设计的的基本原则就是：模板作为一个薄的、类型安全的包装层，
实现层的代码可以多个模板实例来公用，这样不但加快了编译时间，也减小了生成的代码尺寸。

这个实现相当于 std::map<Key1,std::map<Key2,Val> >
但接口上也不完全相同，主要是基于易实现和性能考虑

-# kmapdset<Key1, pair<Key2, Val> > == std::map<Key1,std::map<Key2,Val> >

@param Key
@param Data
   -# Key2 is embeded in \a Data
   -# if Data is std::pair<T1,T2>, T1 will be used as key2_t
   -# else must define Data::key_type as key2_t, and key_type must load/save first in \a Data
     -# Data can be key2_t self

@code

 kmapdset<var_uint32_t, pair<string, var_uint32_t> > kmd(env, "my_kmapdset");
 kmd.insert(100, make_pair("100", 100));
 kmd.insert(100, make_pair("101", 101));
 kmd.insert(100, make_pair("102", 102));
 kmd.replace(100, make_pari("100", 10000));

@endcode
 */
kmapdset_iterator_impl_base::kmapdset_iterator_impl_base(class kmapdset_base* owner)
	: m_owner(owner)
	, m_curp(0), m_ret(-1)
{
	memset(&m_bulk, 0, sizeof(DBT));
	m_bulk.size = (owner->m_bulkSize);
	m_bulk.data = (::malloc(owner->m_bulkSize));
	m_bulk.flags = (DB_DBT_USERMEM);
	m_bulk.ulen = (owner->m_bulkSize);
}

void kmapdset_iterator_impl_base::init(DB* dbp, DB_TXN* txn, const char* func)
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

kmapdset_iterator_impl_base::~kmapdset_iterator_impl_base()
{
	if (m_bulk.data)
		::free(m_bulk.data);
	if (m_curp)
		m_curp->close(m_curp);
}

bool kmapdset_iterator_impl_base::next_key(size_t* cnt, const char* func)
{
	TERARK_RT_assert(0 == m_ret, std::logic_error);
	DBT tk1; memset(&tk1, 0, sizeof(DBT));
	m_ret = m_curp->get(m_curp, &tk1, &m_bulk, DB_NEXT_NODUP);
	if (0 == m_ret)
	{
		load_key1(tk1.data, tk1.size);
		db_recno_t cnt0 = 0;
		int ret = m_curp->count(m_curp, &cnt0, 0);
		if (0 != ret)
		{
			string_appender<> oss;
			oss << db_strerror(ret) << "... at: " << func;
			throw std::runtime_error(oss.str());
		}
		*cnt = cnt0;
		return true;
	}
	else if (DB_NOTFOUND == m_ret)
	{
		return false;
	}
	else
	{
		string_appender<> oss;
		oss << db_strerror(m_ret) << "... at: " << func;
		throw std::runtime_error(oss.str());
	}
}

void kmapdset_iterator_impl_base::bulk_load(DBT* tk1)
{
	TERARK_RT_assert(0 == m_ret, std::logic_error);
	load_key1(tk1->data, tk1->size);
	clear_vec();
	int ret;
	do {
		void  *bptr, *data;
		DB_MULTIPLE_INIT(bptr, &m_bulk);
		assert(NULL != bptr);
		for (;;)
		{
			size_t size = 0;
			DB_MULTIPLE_NEXT(bptr, &m_bulk, data, size);
			if (terark_likely(NULL != bptr))
				this->push_back(data, size);
			else
				break;
		}
		ret = m_curp->get(m_curp, tk1, &m_bulk, DB_MULTIPLE|DB_NEXT_DUP);
	} while (0 == ret);
}

void kmapdset_iterator_impl_base::increment(const char* func)
{
	TERARK_RT_assert(0 == m_ret, std::logic_error);
	DBT tk1; memset(&tk1, 0, sizeof(DBT));
	m_ret = m_curp->get(m_curp, &tk1, &m_bulk, DB_NEXT_NODUP|DB_MULTIPLE);
	if (0 == m_ret)
	{
		bulk_load(&tk1);
	}
	else if (DB_NOTFOUND != m_ret)
	{
		string_appender<> oss;
		oss << db_strerror(m_ret) << "... at: " << func;
		throw std::runtime_error(oss.str());
	}
}

void kmapdset_iterator_impl_base::decrement(const char* func)
{
	TERARK_RT_assert(0 == m_ret, std::logic_error);
	DBT tk1; memset(&tk1, 0, sizeof(DBT));
	m_ret = m_curp->get(m_curp, &tk1, &m_bulk, DB_PREV_NODUP);
	if (0 == m_ret)
	{
		m_ret = m_curp->get(m_curp, &tk1, &m_bulk, DB_CURRENT|DB_MULTIPLE);
		if (0 == m_ret)
		{
			bulk_load(&tk1);
		}
		else if (DB_KEYEMPTY == m_ret)
		{
			string_appender<> oss;
			oss << db_strerror(m_ret)
				<< "... at: " << func;
			throw std::runtime_error(oss.str());
		}
	}
	else if (DB_NOTFOUND != m_ret)
	{
		string_appender<> oss;
		oss << db_strerror(m_ret) << "... at: " << func;
		throw std::runtime_error(oss.str());
	}
}

bool kmapdset_iterator_impl_base::find_pos(const void* k1, const void* k2, bool bulk, const char* func)
{
	PortableDataOutput<AutoGrownMemIO> oKey1, oKey2;
	m_owner->save_key1(oKey1, k1);
	m_owner->save_key2(oKey2, k2);
	DBT tk1; memset(&tk1, 0, sizeof(DBT)); tk1.data = oKey1.begin(); tk1.size = oKey1.tell();
	DBT tk2; memset(&tk2, 0, sizeof(DBT)); tk2.data = oKey2.begin(); tk2.size = oKey2.tell();
	m_ret = m_curp->get(m_curp, &tk1, &tk2, DB_GET_BOTH);
	if (0 == m_ret)
	{
		if (bulk) {
			m_ret = m_curp->get(m_curp, &tk1, &m_bulk, DB_CURRENT|DB_MULTIPLE);
			if (0 == m_ret) {
				bulk_load(&tk1);
				return true;
			}
		} else {
			clear_vec();
			load_key1(tk1.data, tk1.size);
			push_back(tk2.data, tk2.size);
			return true;
		}
	}
	else if (DB_NOTFOUND == m_ret)
	{
		return false;
	}
	string_appender<> oss;
	oss << db_strerror(m_ret)
		<< "... at: " << func
		<< "\n"
		;
	throw std::runtime_error(oss.str());
}

/**
 @brief
 @return true successful inserted
		false fail, (key1, d) existed, and not inserted
 @throw other errors
 */
bool kmapdset_iterator_impl_base::insert(const void* d, const char* func)
{
	TERARK_RT_assert(0 == m_ret || DB_NOTFOUND == m_ret || DB_KEYEXIST == m_ret, std::logic_error);
	PortableDataOutput<AutoGrownMemIO> oKey1, oData;
	this->save_key1(oKey1);
	m_owner->save_data(oData, d);
	DBT tk1; memset(&tk1, 0, sizeof(DBT)); tk1.data = oKey1.begin(); tk1.size = oKey1.tell();
	DBT tdd; memset(&tdd, 0, sizeof(DBT)); tdd.data = oData.begin(); tdd.size = oData.tell();
	int ret = m_curp->put(m_curp, &tk1, &tdd, DB_NODUPDATA);
	if (DB_KEYEXIST == ret)
		return false;
	if (0 == ret)
		return true;
	string_appender<> oss;
	oss << db_strerror(m_ret)
		<< "... at: " << func;
	throw std::runtime_error(oss.str());
}

/**
 @brief
 @return true successful updated
		false (key1, d.key2) did not exist, not updated
 @throw other errors
 */
bool kmapdset_iterator_impl_base::update(const void* d, const char* func)
{
	TERARK_RT_assert(0 == m_ret, std::logic_error);
	PortableDataOutput<AutoGrownMemIO> oKey1, oData;
	this->save_key1(oKey1);
	m_owner->save_data(oData, d);
	DBT tk1; memset(&tk1, 0, sizeof(DBT)); tk1.data = oKey1.begin(); tk1.size = oKey1.tell();
	DBT tdd; memset(&tdd, 0, sizeof(DBT)); tdd.data = oData.begin(); tdd.size = oData.tell();
	int ret = m_curp->get(m_curp, &tk1, &tdd, DB_GET_BOTH);
	if (0 == ret)
	{
		tk1.data = oKey1.begin(); tk1.size = oKey1.tell();
		tdd.data = oData.begin(); tdd.size = oData.tell();
		ret = m_curp->put(m_curp, &tk1, &tdd, DB_CURRENT);
		if (0 == ret)
			return true;
	}
	else if (DB_NOTFOUND == ret)
	{
		return false;
	}
	string_appender<> oss;
	oss << db_strerror(ret)
		<< "... at: " << func;
	throw std::runtime_error(oss.str());
}

/**
 @brief
 @return true  item was replaced by (key1,d)
         false item was inserted
 @throw  other errors
 */
bool kmapdset_iterator_impl_base::replace(const void* d, const char* func)
{
	PortableDataOutput<AutoGrownMemIO> oKey1, oData;
	this->save_key1(oKey1);
	m_owner->save_data(oData, d);
	DBT tk1; memset(&tk1, 0, sizeof(DBT)); tk1.data = oKey1.begin(); tk1.size = oKey1.tell();
	DBT tdd; memset(&tdd, 0, sizeof(DBT)); tdd.data = oData.begin(); tdd.size = oData.tell();
	int ret = m_curp->get(m_curp, &tk1, &tdd, DB_GET_BOTH);
	if (0 == ret)
	{
		tk1.data = oKey1.begin(); tk1.size = oKey1.tell();
		tdd.data = oData.begin(); tdd.size = oData.tell();
		ret = m_curp->put(m_curp, &tk1, &tdd, DB_CURRENT);
		if (0 == ret)
			return true;
	}
	else if (DB_NOTFOUND == ret)
	{
		ret = m_curp->put(m_curp, &tk1, &tdd, DB_NODUPDATA);
		if (0 == ret)
			return false;
	}
	string_appender<> oss;
	oss << db_strerror(ret)
		<< "... at: " << func;
	throw std::runtime_error(oss.str());
}

bool kmapdset_iterator_impl_base::remove(const void* k2, const char* func)
{
	TERARK_RT_assert(0 == m_ret, std::logic_error);
	PortableDataOutput<AutoGrownMemIO> oKey1, oKey2;
	this->save_key1(oKey1);
	m_owner->save_key2(oKey2, k2);
	DBT tk1; memset(&tk1, 0, sizeof(DBT)); tk1.data = oKey1.begin(); tk1.size = oKey1.tell();
	DBT tdd; memset(&tdd, 0, sizeof(DBT)); tdd.data = oKey2.begin(); tdd.size = oKey2.tell();

	int ret = m_curp->get(m_curp, &tk1, &tdd, DB_GET_BOTH);
	if (0 == ret)
		ret = m_curp->del(m_curp, 0);
	if (DB_KEYEMPTY == ret)
		return false;
	if (0 == ret)
		return true;
	string_appender<> oss;
	oss << db_strerror(ret)
		<< "... at: " << func;
	throw std::runtime_error(oss.str());
}

bool kmapdset_iterator_impl_base::remove(const char* func)
{
	TERARK_RT_assert(0 == m_ret, std::logic_error);
	PortableDataOutput<AutoGrownMemIO> oKey1;
	this->save_key1(oKey1);
	DBT tk1; memset(&tk1, 0, sizeof(DBT)); tk1.data = oKey1.begin(); tk1.size = oKey1.tell();
	int ret = m_owner->m_db->del(m_owner->m_db, m_curp->txn, &tk1, 0);
	if (DB_NOTFOUND == ret)
		return false;
	if (0 == ret)
		return true;
	string_appender<> oss;
	oss << db_strerror(ret)
		<< "... at: " << func;
	throw std::runtime_error(oss.str());
}

//////////////////////////////////////////////////////////////////////////

kmapdset_base::kmapdset_base(DB_ENV* env, const char* dbname
	, u_int32_t flags
	, DB_TXN* txn
	, bt_compare_fcn_type bt_comp
	, bt_compare_fcn_type dup_comp
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
		if (dup_comp) {
			m_dup_comp = dup_comp;
			m_db->set_dup_compare(m_db, dup_comp);
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

kmapdset_base::~kmapdset_base()
{
	if (m_db)
		m_db->close(m_db, 0);
}

kmapdset_iterator_impl_base* kmapdset_base::begin_impl(DB_TXN* txn, const char* func)
{
	kmapdset_iterator_impl_base* iter = make_iter();
	iter->init(m_db, txn, func);
	DBT tk1; memset(&tk1, 0, sizeof(DBT));
	iter->m_ret = iter->m_curp->get(iter->m_curp, &tk1, &iter->m_bulk, DB_FIRST|DB_MULTIPLE);
	if (0 == iter->m_ret)
	{
		iter->bulk_load(&tk1);
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

kmapdset_iterator_impl_base* kmapdset_base::end_impl(DB_TXN* txn, const char* func)
{
	kmapdset_iterator_impl_base* iter = make_iter();
	iter->init(m_db, txn, func);
	return iter;
}

kmapdset_iterator_impl_base* kmapdset_base::find_impl(const void* k1, DB_TXN* txn, u_int32_t flags, const char* func)
{
	PortableDataOutput<AutoGrownMemIO> oKey1;
	save_key1(oKey1, k1);
	DBT tk1; memset(&tk1, 0, sizeof(DBT)); tk1.data = oKey1.begin(); tk1.size = oKey1.tell();
	kmapdset_iterator_impl_base* iter = make_iter();
	iter->init(m_db, txn, func);
	iter->m_ret = iter->m_curp->get(iter->m_curp, &tk1, &iter->m_bulk, flags);
	if (0 == iter->m_ret)
	{
		iter->bulk_load(&tk1);
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

kmapdset_iterator_impl_base*
kmapdset_base::find_impl(const void* k1, const void* k2, DB_TXN* txn, bool bulk, const char* func)
{
	kmapdset_iterator_impl_base* iter = make_iter();
	iter->init(m_db, txn, func);
	try {
		bool bRet = iter->find_pos(k1, k2, bulk, func);
		TERARK_UNUSED_VAR(bRet);
		return iter;
	} catch (std::exception& exp) {
		delete iter; iter = 0;
		throw exp;
	}
}

kmapdset_iterator_impl_base* kmapdset_base::upper_bound_impl(const void* k1, DB_TXN* txn, const char* func)
{
	PortableDataOutput<AutoGrownMemIO> oKey1;
	save_key1(oKey1, k1);
	DBT tk1; memset(&tk1, 0, sizeof(DBT)); tk1.data = oKey1.begin(); tk1.size = oKey1.tell();
	kmapdset_iterator_impl_base* iter = make_iter();
	iter->init(m_db, txn, func);
	iter->m_ret = iter->m_curp->get(iter->m_curp, &tk1, &iter->m_bulk, DB_SET_RANGE|DB_MULTIPLE);
	if (0 == iter->m_ret)
	{
		DBT kbak; memset(&kbak, 0, sizeof(DBT)); kbak.data = oKey1.begin(); kbak.size = oKey1.tell();
		int cmp = m_bt_comp(m_db, &kbak, &tk1);
		assert(cmp <= 0);
		if (0 == cmp) {
			iter->m_ret = iter->m_curp->get(iter->m_curp, &tk1, &iter->m_bulk, DB_NEXT_NODUP|DB_MULTIPLE);
			if (0 == iter->m_ret)
				iter->bulk_load(&tk1);
		} else
			iter->bulk_load(&tk1);
	}
	if (0 != iter->m_ret && DB_NOTFOUND != iter->m_ret)
	{
		string_appender<> oss;
		oss << db_strerror(iter->m_ret)
			<< "... at: " << func
			<< "\n"
			;
		throw std::runtime_error(oss.str());
	}
	return iter;
}

size_t kmapdset_base::count_impl(const void* k1, DB_TXN* txn, const char* func)
{
	PortableDataOutput<AutoGrownMemIO> oKey1;
	try {
		save_key1(oKey1, k1);
	}
	catch (const IOException& exp)
	{
		string_appender<> oss;
		oss << exp.what() << "... at: " << func;
		throw std::runtime_error(oss.str());
	}
	DBT tk1; memset(&tk1, 0, sizeof(DBT)); tk1.data = oKey1.begin(); tk1.size = oKey1.tell();
	DBT tdd; memset(&tdd, 0, sizeof(DBT));
	DBC* curp = NULL;
	int ret = m_db->cursor(m_db, txn, &curp, 0);
	if (0 == ret)
	{
		ret = curp->get(curp, &tk1, &tdd, DB_SET);
		db_recno_t count = 0;
		if (0 == ret)
			ret = curp->count(curp, &count, 0);
		else if (DB_NOTFOUND == ret)
			count = 0, ret = 0; // clear error

		curp->close(curp);
		if (0 != ret)
			goto ErrorL;
		return count;
	}
ErrorL:
	string_appender<> oss;
	oss << db_strerror(ret)
		<< "... at: " << func
		<< "\n"
		;
	throw std::runtime_error(oss.str());
}

/**
 @brief insert a record
 @return true success,  no same k1-k2 in db, the record was inserted
		 false failed, has same k1-k2 in db, the record was not inserted, not replaced existing yet
 @throws exception, failed
 */
bool kmapdset_base::insert_impl(const void* k1, const void* d, DB_TXN* txn, const char* func)
{
	PortableDataOutput<AutoGrownMemIO> oKey1, oData;
	try {
		save_key1(oKey1, k1);
		save_data(oData, d);
	}
	catch (const IOException& exp)
	{
		string_appender<> oss;
		oss << exp.what() << "... at: " << func;
		throw std::runtime_error(oss.str());
	}
	DBT tk1; memset(&tk1, 0, sizeof(DBT)); tk1.data = oKey1.begin(); tk1.size = oKey1.tell();
	DBT tdd; memset(&tdd, 0, sizeof(DBT)); tdd.data = oData.begin(); tdd.size = oData.tell();
	int ret = m_db->put(m_db, txn, &tk1, &tdd, DB_NODUPDATA);
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
 @brief replace OR insert a record
 @note
  if not thrown an exception, always success
 @return true replace the record
		 false insert the record
 @throws exception, failed
 */
bool kmapdset_base::replace_impl(const void* k1, const void* d, DB_TXN* txn, const char* func)
{
	PortableDataOutput<AutoGrownMemIO> oKey1, oData;
	try {
		save_key1(oKey1, k1);
		save_data(oData, d);
	}
	catch (const IOException& exp)
	{
		string_appender<> oss;
		oss << exp.what() << "... at: " << func;
		throw std::runtime_error(oss.str());
	}
	DBT tk1; memset(&tk1, 0, sizeof(DBT)); tk1.data = oKey1.begin(); tk1.size = oKey1.tell();
	DBT tdd; memset(&tdd, 0, sizeof(DBT)); tdd.data = oData.begin(); tdd.size = oData.tell();
	DBC* curp = NULL;
	int ret = m_db->cursor(m_db, txn, &curp, 0);
	if (0 == ret)
	{
		ret = curp->get(curp, &tk1, &tdd, DB_GET_BOTH);
		if (0 == ret)
		{
			tk1.data = oKey1.begin(); tk1.size = oKey1.tell();
			tdd.data = oData.begin(); tdd.size = oData.tell();
			ret = curp->put(curp, &tk1, &tdd, DB_CURRENT);
			curp->close(curp);
			if (0 == ret)
				return true;
		}
		else if (DB_NOTFOUND == ret)
		{
			ret = curp->put(curp, &tk1, &tdd, DB_NODUPDATA);
			curp->close(curp);
			if (0 == ret)
				return false;
		}
	}
	string_appender<> oss;
	oss << db_strerror(ret)
		<< "... at: " << func
		<< "\n"
		;
	throw std::runtime_error(oss.str());
}

/**
 @brief
 @return true (k1,k2) existed, remove success
        false (k1,k2) not existed, nothing done
 */
bool kmapdset_base::remove_impl(const void* k1, const void* k2, DB_TXN* txn, const char* func)
{
	PortableDataOutput<AutoGrownMemIO> oKey1, oData;
	oData.resize(4*1024);
	save_key1(oKey1, k1);
	save_key2(oData, k2);
	DBT tk1; memset(&tk1, 0, sizeof(DBT)); tk1.data = oKey1.begin(); tk1.size = oKey1.tell();
	DBT tdd; memset(&tdd, 0, sizeof(DBT)); tdd.data = oData.begin(); tdd.size = oData.tell();
	DBC* curp = NULL;
	int ret = m_db->cursor(m_db, txn, &curp, 0);
	if (0 == ret)
	{
		ret = curp->get(curp, &tk1, &tdd, DB_GET_BOTH);
		if (0 == ret) {
			ret = curp->del(curp, 0);
		}
		curp->close(curp);
		return 0 == ret;
	}
	else
	{
		string_appender<> oss;
		oss << db_strerror(ret)
			<< "... at: " << func
			<< "\n"
			;
		throw std::runtime_error(oss.str());
	}
}

/**
 @brief
 @return true (k1) existed, remove success
        false (k1) not existed, nothing done
 */
bool kmapdset_base::remove_impl(const void* k1, DB_TXN* txn, const char* func)
{
	PortableDataOutput<AutoGrownMemIO> oKey1;
	save_key1(oKey1, k1);
	DBT tk1; memset(&tk1, 0, sizeof(DBT)); tk1.data = oKey1.begin(); tk1.size = oKey1.tell();
	int ret = m_db->del(m_db, txn, &tk1, 0);
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

void kmapdset_base::clear_impl(DB_TXN* txn, const char* func)
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

void kmapdset_base::flush_impl(const char* func)
{
	int ret = m_db->sync(m_db, 0);
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


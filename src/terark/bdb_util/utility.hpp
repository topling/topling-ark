#ifndef __terark_bdb_util_utility_h__
#define __terark_bdb_util_utility_h__

/**
 @file 几个便于使用的 BDB 实用类

  db_cxx.h 中的 Dbc, DbTxn 类不是使用析构函数来释放资源和提交更改
  如果需要这些功能，请使用 terark::DbCursor, terark::DbTxnGuard

  db_cxx.h 中的 Dbt 需要自己设置 data/size
  DbtRaw/DbtUserMem 自动识别并设置 data/size 以及 flag/ulen

 */

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#if defined(_WIN32) || defined(_WIN64)
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif
#endif

#include <db_cxx.h>
#include <terark/config.hpp>

namespace terark {

class DbtRaw : public Dbt
{
public:
	template<class RawData>
	DbtRaw(RawData& d, u_int32_t flags = 0)
		: Dbt(&d, sizeof(RawData))
	{
		if (flags & DB_DBT_USERMEM)
		{
			set_ulen(sizeof(RawData));
		}
		set_flags(flags);
	}
	DbtRaw(void* data, u_int32_t size, u_int32_t flags = 0)
		: Dbt(data, size)
	{
		if (flags & DB_DBT_USERMEM)
		{
			set_ulen(size);
		}
		set_flags(flags);
	}
};

class DbtUserMem : public Dbt
{
public:
	template<class RawData>
	DbtUserMem(RawData& d)
		: Dbt(&d, sizeof(RawData))
	{
		set_flags(DB_DBT_USERMEM);
		set_ulen(sizeof(RawData));
	}
	DbtUserMem(void* data, u_int32_t size, u_int32_t flags = 0)
		: Dbt(data, size)
	{
		set_flags(DB_DBT_USERMEM|flags);
		set_ulen(size);
	}
};


class TERARK_DLL_EXPORT DbTxnGuard
{
	DbTxn* m_txn;
	bool m_aborted;
	bool m_commited;
	bool m_prepared;
	bool m_discarded;

	DbTxnGuard(const DbTxnGuard&); // non-copyable

public:
	DbTxnGuard(DbEnv* env, DbTxnGuard* parent = 0, u_int32_t flags = 0);
	~DbTxnGuard();

	//! 在成功的路径上调用 commit
	int commit(u_int32_t flags = 0);

	//! 如果发生了异常，会自动 abort，也可以显式 abort
	int abort();

	//! 如果发生了异常，会自动 discard，也可以显式 discard
	int discard(u_int32_t flags);

	int prepare(u_int8_t *gid);

	operator DbTxn*() { return m_txn; }

//////////////////////////////////////////////////////////////////////////

	u_int32_t id() { return m_txn->id(); }
	int get_name(const char **namep) { return m_txn->get_name(namep); }
	int set_name(const char *name) { return m_txn->set_name(name); }
	int set_timeout(db_timeout_t timeout, u_int32_t flags) { return m_txn->set_timeout(timeout, flags); }

	DbTxn* get_DbTxn() { return m_txn; }

	DB_TXN *get_DB_TXN() { return m_txn->get_DB_TXN(); }

	const DB_TXN *get_const_DB_TXN() const { return m_txn->get_const_DB_TXN(); }
};

class TERARK_DLL_EXPORT DbCursor
{
	Dbc* m_cursor;

public:
	DbCursor(Db* db, DbTxn* txn, u_int32_t flags = 0);
	DbCursor(const DbCursor& y);
	const DbCursor& operator=(const DbCursor& y);

	void swap(DbCursor& y) { Dbc* t = m_cursor; m_cursor = y.m_cursor; y.m_cursor = t; }

	~DbCursor();

	int count(db_recno_t *countp, u_int32_t flags) { return m_cursor->count(countp, flags); }
	int del(u_int32_t flags) { return m_cursor->del(flags); }
	int get(Dbt* key, Dbt *data, u_int32_t flags) { return m_cursor->get(key, data, flags); }
	int get_priority(DB_CACHE_PRIORITY *priorityp) { return m_cursor->get_priority(priorityp); }
	int pget(Dbt* key, Dbt* pkey, Dbt *data, u_int32_t flags) { return m_cursor->pget(key, pkey, data, flags); }
	int put(Dbt* key, Dbt *data, u_int32_t flags) { return m_cursor->put(key, data, flags); }
	int set_priority(DB_CACHE_PRIORITY priority) { return m_cursor->set_priority(priority); }
};

} // namespace terark


#endif // __terark_bdb_util_utility_h__


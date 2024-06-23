#include "utility.hpp"
#include <assert.h>

namespace terark {

DbTxnGuard::DbTxnGuard(DbEnv* env, DbTxnGuard* parent, u_int32_t flags)
{
	int ret = env->txn_begin(parent ? parent->m_txn : NULL, &m_txn, flags);
	TERARK_UNUSED_VAR(ret);
	m_aborted = false;
	m_commited = false;
	m_prepared = false;
	m_discarded = false;
}

int DbTxnGuard::commit(u_int32_t flags)
{
	int ret = m_txn->commit(flags);
	m_commited = true;
	return ret;
}

int DbTxnGuard::abort()
{
	assert(!m_aborted && !m_commited && !m_discarded);

	int ret = m_txn->abort();
	if (0 == ret)
	{
		m_aborted = true;
	}
	return ret;
}

int DbTxnGuard::discard(u_int32_t flags)
{
	assert(!m_aborted && !m_commited && !m_discarded);
	int ret = m_txn->discard(flags);
	if (0 == ret)
	{
		m_discarded = true;
	}
	return ret;
}

int DbTxnGuard::prepare(u_int8_t *gid)
{
	assert(!m_aborted && !m_commited && !m_discarded && !m_prepared);
	int ret = m_txn->prepare(gid);
	if (0 == ret)
	{
		m_prepared = true;
	}
	return ret;
}

DbTxnGuard::~DbTxnGuard()
{
	if (m_aborted)
		return;
	if (m_commited)
		return;
	if (m_prepared && !m_discarded)
		m_txn->discard(0);
	else
		m_txn->abort();
}

//////////////////////////////////////////////////////////////////////////

DbCursor::DbCursor(Db* db, DbTxn* txn, u_int32_t flags)
{
	int ret = db->cursor(txn, &m_cursor, flags);
	TERARK_UNUSED_VAR(ret);
}

const DbCursor& DbCursor::operator=(const DbCursor& y)
{
	m_cursor->close();
	int ret = y.m_cursor->dup(&m_cursor, DB_POSITION);
	TERARK_UNUSED_VAR(ret);
	return *this;
}

DbCursor::DbCursor(const DbCursor& y)
{
	int ret = y.m_cursor->dup(&m_cursor, DB_POSITION);
	TERARK_UNUSED_VAR(ret);
}

DbCursor::~DbCursor()
{
	m_cursor->close();
}


} // namespace terark

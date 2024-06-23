#ifndef __terark_bdb_util_persistent_alloc_h__
#define __terark_bdb_util_persistent_alloc_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#if defined(_WIN32) || defined(_WIN64)
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif
#endif

#include <terark/stdtypes.hpp>
#include <boost/function.hpp>
#include <string>
#include <iostream>

#include <db_cxx.h>
#include "utility.hpp"
#include <terark/io/byte_swap.hpp>

namespace terark {

template<class PositionT, class SizeT>
class HugeChunkAllocator
{
public:
	virtual ~HugeChunkAllocator() {}
	virtual std::pair<PositionT,SizeT> chunk_alloc(SizeT size) = 0;
};

template<class PositionT, class SizeT>
class DummyHugeChunkAllocator : public HugeChunkAllocator<PositionT,SizeT>
{
public:
	PositionT m_lower;
	PositionT m_upper;
	PositionT m_cur;

	DummyHugeChunkAllocator(PositionT lower, PositionT upper)
	{
		m_lower = lower;
		m_upper = upper;
		m_cur = lower;
	}
	virtual std::pair<PositionT,SizeT> chunk_alloc(SizeT size)
	{
		PositionT pos = m_cur;
		m_cur += size;
		return std::make_pair(pos, size);
	}
};

template<class PositionT, class SizeT>
class BasicPersistentAllocator
{
public:
	typedef PositionT position_type;
	typedef SizeT     size_type;
	typedef std::pair<PositionT, SizeT> pos_size_t;
	typedef HugeChunkAllocator<PositionT,SizeT> hc_alloc_t;

	explicit BasicPersistentAllocator(
						DbEnv* env = NULL,
						const std::string& prefix = "TOAST",
						SizeT cell_size = 16,
						SizeT init_space_size = 0,
						hc_alloc_t* hc_alloc = 0,
						bool bCreate = true);

	~BasicPersistentAllocator();

	PositionT alloc(SizeT size);

	PositionT alloc(PositionT old_pos,
					SizeT  old_size,
					SizeT  min_size,
					SizeT* max_size);

	void free(PositionT pos, SizeT size);

	void print_all(std::ostream& os = std::cout);
	void print_free_ptr(std::ostream& os = std::cout);
	void print_spac_tab(std::ostream& os = std::cout);
	void print_used_ptr(std::ostream& os = std::cout);

	void print_trace();
	void verify();

private:
	void print_ptr(const char* szHead, Db& tab, std::ostream& os);
	PositionT alloc_new(DbTxnGuard& txn, SizeT min_size, SizeT* max_size);
	std::pair<PositionT,SizeT> make_new_space(DbTxnGuard& txn, SizeT new_size);

private:
	// non-copyable
	BasicPersistentAllocator(const BasicPersistentAllocator& rhs);

	hc_alloc_t* m_hc_alloc;
	bool m_own_hc_alloc;

	const SizeT m_cell_size;

	DbEnv* m_env;
	Db m_free_ptr;
	Db m_spac_tab;
	Db m_used;
	std::string m_prefix;

private:
	static int PositionT_compare(DB *db, const DBT *dbt1, const DBT *dbt2)
	{
		PositionT p1, p2;
		memcpy(&p1, dbt1->data, sizeof(PositionT));
		memcpy(&p2, dbt2->data, sizeof(PositionT));
#ifdef BOOST_LITTLE_ENDIAN
		byte_swap(p1);
		byte_swap(p2);
#endif
		if (p1 < p2) return -1;
		if (p2 < p1) return  1;
		return 0;
	}

	static int SizeT_compare(DB *db, const DBT *dbt1, const DBT *dbt2)
	{
		SizeT n1, n2;
		memcpy(&n1, dbt1->data, sizeof(SizeT));
		memcpy(&n2, dbt2->data, sizeof(SizeT));
#ifdef BOOST_LITTLE_ENDIAN
		byte_swap(n1);
		byte_swap(n2);
#endif
		if (n1 < n2) return -1;
		if (n2 < n1) return  1;
		return 0;
	}
};

typedef BasicPersistentAllocator<stream_position_t, uint32_t> PersistentAllocator_LL;

} // namespace terark


#endif // __terark_bdb_util_persistent_alloc_h__


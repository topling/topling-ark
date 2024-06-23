#include "persistent_alloc.hpp"
#include "native_compare.hpp"
#include "utility.hpp"
#include <terark/num_to_str.hpp>
#include <fstream>
#include <iomanip>
#include <vector>
#include <algorithm>

namespace terark {

template<class PositionT, class SizeT>
class DefaultHugeChunkAllocator : public HugeChunkAllocator<PositionT,SizeT>
{
public:
	virtual std::pair<PositionT,SizeT> chunk_alloc(SizeT size)
	{
		return std::make_pair(0, 0);
	}
};

template<class PositionT, class SizeT>
BasicPersistentAllocator<PositionT, SizeT>::BasicPersistentAllocator(
							DbEnv* env,
							const std::string& prefix,
							SizeT cell_size,
							SizeT init_space_size,
							hc_alloc_t* hc_alloc,
							bool bCreate)
 : m_cell_size(cell_size)
 , m_env(env)
 , m_free_ptr(env, 0)
 , m_spac_tab(env, 0)
 , m_used(env, 0)
{
	m_free_ptr.set_bt_compare(&BasicPersistentAllocator::PositionT_compare);
	m_free_ptr.set_bt_prefix(&bdb_fixed_key_prefix);

	m_spac_tab.set_bt_compare(&BasicPersistentAllocator::SizeT_compare);
	m_spac_tab.set_bt_prefix(&bdb_fixed_key_prefix);
	m_spac_tab.set_dup_compare(&BasicPersistentAllocator::PositionT_compare);

	m_used.set_bt_compare(&BasicPersistentAllocator::PositionT_compare);
	m_used.set_bt_prefix(&bdb_fixed_key_prefix);

	if (hc_alloc) {
		m_hc_alloc = hc_alloc;
		m_own_hc_alloc = false;
	} else {
		m_hc_alloc = new DefaultHugeChunkAllocator<PositionT,SizeT>;
		m_own_hc_alloc = true;
	}
	std::string strFreePtr, strSpacTab, strUsed;
	strFreePtr += prefix;
	strFreePtr += "_free_ptr.db";

	strSpacTab += prefix;
	strSpacTab += "_space_tab.db";

	strUsed += prefix;
	strUsed += "_used.db";

	int ret;
	bool bExisted = true;
	int flags = DB_AUTO_COMMIT|DB_MULTIVERSION;
	try {
		ret = m_free_ptr.open(
			NULL,       /* Transaction pointer */
			strFreePtr.c_str(), /* On-disk file that holds the database. */
			NULL,       /* Optional logical database name */
			DB_BTREE,   /* Database access method */
			flags,  /* Open flags */
			0);         /* File mode (using defaults) */
	} catch (const DbException& ) {
		bExisted = false;
	//	flags |= DB_TRUNCATE;
		flags |= DB_CREATE;
		ret = m_free_ptr.open(
			NULL,       /* Transaction pointer */
			strFreePtr.c_str(), /* On-disk file that holds the database. */
			NULL,       /* Optional logical database name */
			DB_BTREE,   /* Database access method */
			flags,  /* Open flags */
			0);         /* File mode (using defaults) */
	}
	ret = m_spac_tab.open(
		NULL,       /* Transaction pointer */
		strUsed.c_str(), /* On-disk file that holds the database. */
		NULL,       /* Optional logical database name */
		DB_BTREE,   /* Database access method */
		flags,  /* Open flags */
		0);         /* File mode (using defaults) */

	ret = m_used.open(
		NULL,       /* Transaction pointer */
		strSpacTab.c_str(), /* On-disk file that holds the database. */
		NULL,       /* Optional logical database name */
		DB_BTREE,   /* Database access method */
		flags,  /* Open flags */
		0);         /* File mode (using defaults) */

	if (!bExisted)
	{
		DbTxnGuard txn(m_env);

		std::pair<PositionT,SizeT> pos_size = make_new_space(txn, init_space_size);
		DbtRaw dpos(pos_size.first);
		DbtRaw dsize(pos_size.second);

		ret = m_free_ptr.put(txn, &dpos, &dsize, 0); assert(0 == ret);
		ret = m_spac_tab.put(txn, &dsize, &dpos, 0); assert(0 == ret);

		txn.commit();
	}
	TERARK_UNUSED_VAR(ret);
}

template<class PositionT, class SizeT>
BasicPersistentAllocator<PositionT, SizeT>::~BasicPersistentAllocator()
{
	if (m_own_hc_alloc)
		delete m_hc_alloc;

	m_free_ptr.close(0);
	m_spac_tab.close(0);
	m_used.close(0);
}

template<class PositionT, class SizeT>
std::pair<PositionT,SizeT>
BasicPersistentAllocator<PositionT, SizeT>::make_new_space(DbTxnGuard& txn, SizeT new_size)
{
	using namespace std;

	const std::pair<PositionT,SizeT> pos_size = m_hc_alloc->chunk_alloc(new_size);
	if (pos_size.second < new_size)
	{
		string_appender<> oss;
		oss << "make_new_space must return sufficient space, or throw exception."
			<< " space[req=" << new_size << ", ret=" << pos_size.second << "]";
		throw std::logic_error(oss.str().c_str());
	}
	DbCursor next_ptr(&m_free_ptr, txn);

	int next_ret, prev_ret;

	PositionT next_pos = pos_size.first; SizeT next_size = pos_size.second;
	DbtUserMem next_pos_d(next_pos), next_size_d(next_size);

	next_ret = next_ptr.get(&next_pos_d, &next_size_d, DB_SET_RANGE);// assert(0 == next_ret);

	PositionT prev_pos; SizeT prev_size;
	DbtUserMem prev_pos_d(prev_pos), prev_size_d(prev_size);
	DbCursor prev_ptr(next_ptr);
	prev_ret = prev_ptr.get(&prev_pos_d, &prev_size_d, DB_PREV);     // assert(0 == prev_ret);

	if ((0 == prev_ret && pos_size.first < prev_pos + prev_size) ||
		(0 == next_ret && next_pos < pos_size.first + pos_size.second))
	{
		// 返回的空间和已存在的空闲块重叠，这说明 make_new_space 的实现有误
		//
		string_appender<> oss;
		oss << "make_new_space return space is already in free list."
			<< " space[pos=" << pos_size.first << ", size=" << pos_size.second << "]";
		throw std::logic_error(oss.str().c_str());
	}

	return pos_size;
}

template<class PositionT, class SizeT>
PositionT BasicPersistentAllocator<PositionT, SizeT>::alloc_new(DbTxnGuard& txn, SizeT min_size, SizeT* max_size)
{
	using namespace std;

	assert( min_size % m_cell_size == 0);
	assert(*max_size % m_cell_size == 0);

	PositionT free_pos = 0; SizeT free_size = min_size;
	DbtUserMem dpos(free_pos), dsize(free_size);

	DbCursor spacep(&m_spac_tab, txn);
	int ret = spacep.get(&dsize, &dpos, DB_SET_RANGE);
	if (0 == ret)
	{ // found sufficient free space
		ret = spacep.del(0);  assert(0 == ret);
		ret = m_free_ptr.del(txn, &dpos, 0);  assert(0 == ret);

		if (free_size > *max_size) {
			PositionT new_free_pos  = free_pos  + *max_size;
			SizeT     new_free_size = free_size - *max_size;
			DbtRaw dpos2(new_free_pos), dsize2(new_free_size);
			ret = m_free_ptr.put(txn, &dpos2, &dsize2, DB_NOOVERWRITE); assert(0 == ret);
			ret = m_spac_tab.put(txn, &dsize2, &dpos2, 0); assert(0 == ret);
		} else
			*max_size = free_size;
	}
	else // not found free space with sufficient size
	{
		// make new space
		//
		std::pair<PositionT,SizeT> pos_size = make_new_space(txn, min_size);
		free_pos = pos_size.first;
		if (pos_size.second <= *max_size)
		{
			*max_size = pos_size.second;
		}
		else // has more space, put it to free list
		{
			pos_size.first += *max_size;
			pos_size.second -= *max_size;
			DbtRaw dpos2(pos_size.first), dsize2(pos_size.second);
			ret = m_free_ptr.put(txn, &dpos2, &dsize2, DB_NOOVERWRITE); assert(0 == ret);
			ret = m_spac_tab.put(txn, &dsize2, &dpos2, 0); assert(0 == ret);
		}
	}
	return free_pos;
}

template<class PositionT, class SizeT>
PositionT BasicPersistentAllocator<PositionT, SizeT>::alloc(SizeT size)
{
	DbTxnGuard txn(m_env);
	SizeT max_size = size = align_up(size, m_cell_size);
	PositionT pos = alloc_new(txn, size, &max_size);
	assert(max_size == size);

	DbtRaw dpos(pos), dsize(size);
	int ret = m_used.put(txn, &dpos, &dsize, DB_NOOVERWRITE); assert(0 == ret);

	ret = txn.commit(0); assert(0 == ret);
	TERARK_UNUSED_VAR(ret);
	return pos;
}

/**
 @brief 分配空间

  如果 old_pos 之后紧接的有空闲空间，就返回 old_pos
  否则返回新分配的块，old_pos 不会被释放，调用者应该在随后释放 old_pos
 */
template<class PositionT, class SizeT>
PositionT BasicPersistentAllocator<PositionT, SizeT>::alloc(PositionT old_pos,
													 SizeT  old_size,
													 SizeT  min_size,
													 SizeT* max_size)
{
	using namespace std;
	PositionT ret_pos;

	old_size = align_up(old_size, m_cell_size);
	min_size = align_up(min_size, m_cell_size);
	*max_size = align_up(*max_size, m_cell_size);

	int ret = 0;
	DbTxnGuard txn(m_env);
	if (min_size == old_size)
	{
		assert(*max_size == min_size);
		*max_size = min_size;
		ret_pos = old_pos;
	}
	else if (*max_size < old_size)
	{
		DbtRaw pos_d(old_pos);
		DbtRaw size_d(*max_size);
		ret = m_used.put(txn, &pos_d, &size_d, 0); assert(0 == ret);

		PositionT free_pos  = old_pos  + *max_size;
		SizeT     free_size = old_size - *max_size;
		pos_d.set_data(&free_pos);
		size_d.set_data(&free_size);
		ret = m_free_ptr.put(txn, &pos_d, &size_d, 0); assert(0 == ret);
		ret = m_spac_tab.put(txn, &size_d, &pos_d, 0); assert(0 == ret);
		ret_pos = old_pos;
	}
	else
	{
	// 查找紧跟在 old_pos 之后的空闲块
	PositionT next_free_pos = old_pos + old_size;
	SizeT     next_free_size;
	DbtUserMem next_free_pos_d(next_free_pos);
	DbtUserMem next_free_size_d(next_free_size);

	bool hasSufficient = false;
	DbCursor pos_cur(&m_free_ptr, txn);
	ret = pos_cur.get(&next_free_pos_d, &next_free_size_d, DB_SET);
	if (0 == ret)
	{ // (old_pos, old_size) 与后一个空闲块相邻
		assert(old_pos + old_size == next_free_pos);

		DbCursor spc_cur(&m_spac_tab, txn);
		ret = spc_cur.get(&next_free_size_d, &next_free_pos_d, DB_GET_BOTH);
		if (0 != ret)
		{
			string_appender<> oss;
			oss << "fatal: " << __FILE__ << ":" << __LINE__ << ", 未找到相应的 space 项目" << BOOST_CURRENT_FUNCTION;
			throw std::runtime_error(oss.str().c_str());
		}
		if (old_size + next_free_size >= min_size)
		{
			hasSufficient = true;
			ret = pos_cur.del(0); assert(0 == ret);
			ret = spc_cur.del(0); assert(0 == ret);
			if (old_size + next_free_size > *max_size)
			{ // 从这个空闲块前部划分一部分，后面的部分仍空闲
				next_free_pos = old_pos + *max_size;
				next_free_size -= (*max_size - old_size);
				ret = m_free_ptr.put(txn, &next_free_pos_d, &next_free_size_d, DB_NOOVERWRITE); assert(0 == ret);
				ret = m_spac_tab.put(txn, &next_free_size_d, &next_free_pos_d, 0); assert(0 == ret);
			} else
				*max_size = SizeT(old_size + next_free_size);
			ret_pos = old_pos;
		}
	}
	if (!hasSufficient)
	{
		DbtRaw old_pos_d(old_pos), old_size_d(old_size);
		ret = m_free_ptr.put(txn, &old_pos_d, &old_size_d, DB_NOOVERWRITE); assert(0 == ret);
		ret = m_spac_tab.put(txn, &old_size_d, &old_pos_d, 0); assert(0 == ret);
		ret_pos = alloc_new(txn, min_size, max_size);
	}
	{
		if (ret_pos != old_pos)
		{
			DbtRaw old_pos_d(old_pos);
			ret = m_used.del(txn, &old_pos_d, 0); assert(0 == ret);
		}
		DbtRaw ret_pos_d(ret_pos), ret_size_d(*max_size);
		ret = m_used.put(txn, &ret_pos_d, &ret_size_d, 0); assert(0 == ret);
	}
	}
	ret = txn.commit(0); assert(0 == ret);

	return ret_pos;
}

/**
 @brief 简易说明

 @param pos  要释放的空间地址
 @param size 这个空间的大小，必须正确提供这个大小（当初申请的大小）
 */
template<class PositionT, class SizeT>
void BasicPersistentAllocator<PositionT, SizeT>::free(PositionT pos, SizeT size0)
{
	using namespace std;

	SizeT size = align_up(size0, m_cell_size);

	int ret;
	DbTxnGuard txn(m_env);
	{
	PositionT free_pos = pos; SizeT free_size = size;
	PositionT next_pos = pos; SizeT next_size = size;
	DbtUserMem next_pos_d(next_pos), next_size_d(next_size);

	DbCursor next_cur_ptr(&m_free_ptr, txn);
	int next_ret = next_cur_ptr.get(&next_pos_d, &next_size_d, DB_SET_RANGE);

	PositionT prev_pos; SizeT prev_size;
	DbtUserMem prev_pos_d(prev_pos), prev_size_d(prev_size);

	DbCursor prev_cur_ptr(next_cur_ptr);
	int prev_ret = prev_cur_ptr.get(&prev_pos_d, &prev_size_d, DB_PREV);
	if (0 == prev_ret)
	{
		assert(prev_pos < free_pos);
		if (prev_pos + prev_size == pos)
		{
			// 与前一个空闲块相邻
			DbCursor prev_cur_spc(&m_spac_tab, txn);
			PositionT prev_pos2 = prev_pos; SizeT prev_size2 = prev_size;
			prev_ret = prev_cur_spc.get(&prev_size_d, &prev_pos_d, DB_GET_BOTH);
			assert(prev_pos2 == prev_pos && prev_size2 == prev_size);
			TERARK_UNUSED_VAR(prev_pos2);
			TERARK_UNUSED_VAR(prev_size2);
			if (DB_NOTFOUND == prev_ret)
			{
				string_appender<> oss;
				oss << "fatal: " << __FILE__ << ":" << __LINE__ << ", 未找到相应的 space 项目" << BOOST_CURRENT_FUNCTION;
				throw std::runtime_error(oss.str().c_str());
			}
			// 合并前一个空闲块
			free_pos = prev_pos;
			free_size += prev_size;
			ret = prev_cur_ptr.del(0); assert(0 == ret);
			ret = prev_cur_spc.del(0); assert(0 == ret);
		}
		else if (prev_pos + prev_size > pos)
		{ // 要释放的块与前一个空闲块重叠
			string_appender<> oss;
			oss << "fatal: " << __FILE__ << ":" << __LINE__ << ", 要释放的块与前一个空闲块重叠:" << BOOST_CURRENT_FUNCTION;
			print_trace();
			verify();
			throw std::runtime_error(oss.str().c_str());
		}
	}

	if (0 == next_ret)
	{
		assert(free_pos <= pos);
		assert(next_pos >= pos);

		if (next_pos == pos)
		{ // 要释放的 pos 已在空闲表中
			string_appender<> oss;
			oss << "fatal: " << __FILE__ << ":" << __LINE__ << ", 要释放的 pos 已在空闲表中:" << BOOST_CURRENT_FUNCTION;
			throw std::runtime_error(oss.str().c_str());
		}
		else if (next_pos < pos + size)
		{
			string_appender<> oss;
			oss << "fatal: " << __FILE__ << ":" << __LINE__
				<< ", 要释放的 pos 与后面的空闲空间重叠: "
				<< "curr[ptr=" << pos << ", size=" << size << "], "
				<< "next[ptr=" << next_pos << ", size=" << next_size << "]\n"
				<< "in func:" << BOOST_CURRENT_FUNCTION;
			std::cerr << oss.str() << std::endl;
			throw std::runtime_error(oss.str().c_str());
		}
		else if (pos + size == next_pos)
		{ // 与后一个空闲块相邻
			assert(free_pos + free_size == pos + size);

			DbCursor next_cur_spc(&m_spac_tab, txn);
			next_ret = next_cur_spc.get(&next_size_d, &next_pos_d, DB_GET_BOTH);
			if (0 != next_ret)
			{
				string_appender<> oss;
				oss << "fatal: " << __FILE__ << ":" << __LINE__ << ", 未找到相应的 space 项目" << BOOST_CURRENT_FUNCTION;
				throw std::runtime_error(oss.str().c_str());
			}
			// 合并后一个空闲块
			free_size += next_size;
			ret = next_cur_ptr.del(0); assert(0 == ret);
			ret = next_cur_spc.del(0); assert(0 == ret);
		}
	}

	DbtRaw free_pos_d(free_pos), free_size_d(free_size);
	ret = m_free_ptr.put(txn, &free_pos_d, &free_size_d, DB_NOOVERWRITE); assert(0 == ret);
	ret = m_spac_tab.put(txn, &free_size_d, &free_pos_d, 0); assert(0 == ret);
	}
	{
		DbtRaw pos_d(pos);
		ret = m_used.del(txn, &pos_d, 0); assert(0 == ret);
	}
	ret = txn.commit(0); assert(0 == ret);
	TERARK_UNUSED_VAR(ret);
}

template<class PositionT, class SizeT>
void BasicPersistentAllocator<PositionT, SizeT>::print_all(std::ostream& os)
{
	os << "free_ptr:\n";
	print_free_ptr(os);

	os << "spac_tab:\n";
	print_spac_tab(os);

	os << "used_tab:\n";
	print_used_ptr(os);
}

template<class PositionT, class SizeT>
void BasicPersistentAllocator<PositionT, SizeT>::print_ptr(const char* szHead, Db& tab, std::ostream& os)
{
	os << szHead << "\n";
	Dbc* curp;
	tab.cursor(NULL, &curp, 0);
	PositionT pos;
	SizeT size;
	DbtUserMem dpos(pos);
	DbtUserMem dsize(size);
	int ret = curp->get(&dpos, &dsize, DB_FIRST);
	int nth = 0;
	while (0 == ret)
	{
		++nth;
		os << std::setw(10) << std::right << pos  << ", ";
		os << std::setw(10) << std::right << (pos+size) << ", ";
		os << std::setw( 8) << std::right << size << "\n";
		ret = curp->get(&dpos, &dsize, DB_NEXT);
	}
	curp->close();
}

template<class PositionT, class SizeT>
void BasicPersistentAllocator<PositionT, SizeT>::print_free_ptr(std::ostream& os)
{
	print_ptr("Free_Pos, Size", m_free_ptr, os);
}

template<class PositionT, class SizeT>
void BasicPersistentAllocator<PositionT, SizeT>::print_spac_tab(std::ostream& os)
{
	os << "Size,     Free_Pos\n";
	Dbc* curp;
	m_spac_tab.cursor(NULL, &curp, 0);
	PositionT pos;
	SizeT size;
	DbtUserMem dpos(pos);
	DbtUserMem dsize(size);
	int ret = curp->get(&dsize, &dpos, DB_FIRST);
	int nth = 0;
	while (0 == ret)
	{
		++nth;
		os << std::setw( 8) << std::right << size << ", ";
		os << std::setw(10) << std::right << pos  << "\n";
		ret = curp->get(&dsize, &dpos, DB_NEXT);
	}
	curp->close();
}

template<class PositionT, class SizeT>
void BasicPersistentAllocator<PositionT, SizeT>::print_used_ptr(std::ostream& os)
{
	print_ptr("Used_Pos, Size", m_used, os);
}

template<class PositionT, class SizeT>
void BasicPersistentAllocator<PositionT, SizeT>::print_trace()
{
	std::ofstream fptrp("trace_ptrp.txt");
	std::ofstream fspac("trace_spac.txt");
	std::ofstream fused("trace_used.txt");
	print_free_ptr(fptrp);
	print_spac_tab(fspac);
	print_used_ptr(fused);
}

template<class PositionT, class SizeT>
void BasicPersistentAllocator<PositionT, SizeT>::verify()
{
	using namespace std;
	vector<pair<PositionT,SizeT> > ptrp, used, spac, all;

	PositionT pos; SizeT size;
	DbtUserMem dpos(pos), dsize(size);
	{
		DbCursor curp(&m_spac_tab, NULL);
		int ret = curp.get(&dsize, &dpos, DB_FIRST);
		while (0 == ret)
		{
			spac.push_back(make_pair(pos,size));
			ret = curp.get(&dsize, &dpos, DB_NEXT);
		}
	}
	{
		DbCursor curp(&m_free_ptr, NULL);
		int ret = curp.get(&dpos, &dsize, DB_FIRST);
		while (0 == ret)
		{
			ptrp.push_back(make_pair(pos,size));
			ret = curp.get(&dpos, &dsize, DB_NEXT);
		}
	}
	{
		DbCursor curp(&m_used, NULL);
		int ret = curp.get(&dpos, &dsize, DB_FIRST);
		while (0 == ret)
		{
			used.push_back(make_pair(pos,size));
			ret = curp.get(&dpos, &dsize, DB_NEXT);
		}
	}
	sort(spac.begin(), spac.end());

	set_union(ptrp.begin(), ptrp.end(), used.begin(), used.end(), back_inserter(all));

	assert(ptrp.size() == spac.size());
	for (size_t i = 0; i != ptrp.size(); ++i)
	{
		assert(ptrp[i] == spac[i]);
	}
	for (size_t i = 0; i < all.size()-1; ++i)
	{
		PositionT curr_upper = all[i+0].first + all[i].second;
		PositionT next_lower = all[i+1].first;
		if (curr_upper != next_lower)
		{
			cerr<< "error, all[" << setw(2) << i << "]: curr["
				<< setw(8) << all[i].first
				<< " , " << setw(8) << (all[i+0].first + all[i+0].second)
				<< " , " << setw(8) << all[i+0].second << "]; next["
				<< setw(8) << all[i+1].first
				<< " , " << setw(8) << (all[i+1].first + all[i+1].second)
				<< " , " << setw(8) << all[i+1].second << "]"
				;
			cerr<<endl;
		}
	}
}

template class BasicPersistentAllocator<stream_position_t, uint32_t>;

} // namespace terark


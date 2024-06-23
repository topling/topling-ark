/* vim: set tabstop=4 : */
#ifndef __terark_serialize_common_h__
#define __terark_serialize_common_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#include <terark/io/DataIO.hpp>
#include <terark/set_op.hpp>

namespace terark { namespace prefix_zip {

//////////////////////////////////////////////////////////////////////////

template<class SrcTableT,
//		 class ValueCompare,
		 class Output>
void dump_save_to_merged_impl(const SrcTableT& src,
//							  ValueCompare vcomp, // can be nil
							  Output& output,
							  typename SrcTableT::fixed_index::dump_header& header)
{
	typename SrcTableT::fixed_index temp(TERARK_IF_DEBUG(8*1024, 2*1024*1024)); // small for best debug...

	output << header; // save prepared header...
	header = typename SrcTableT::fixed_index::dump_header(); // reset header

	int segment_count = 0;
	uint32_t nthKey = 0;
	typename SrcTableT::const_iterator iEnd = src.end();
	for (typename SrcTableT::const_iterator iter = src.begin(); iter != iEnd; ++nthKey)
	{
		typename SrcTableT::key_type text = src.key(iter);
	//	typename SrcTableT::c_val_iter_t ilow = iter.begin(vcomp);
		typename SrcTableT::const_iterator ilow = iter;
		size_t equal_count = src.goto_next_larger(iter);
		assert(equal_count > 0);
	//	typename SrcTableT::c_val_iter_t iupp = iter.begin(vcomp);
		typename SrcTableT::const_iterator iupp = iter;

		bool zpool_is_full = false;
		try {
			if (1 == equal_count)
				temp.push_back(text, *ilow);
			else
				temp.push_back(text, ilow, iupp, equal_count);
		}
		catch (const OutOfSpaceException& /*exp*/)
		{
//#if defined(_DEBUG) || defined(DEBUG) || !defined(NDEBUG)
			printf("dump_save_to_merged: key_pool is full, save segment %3d.\n", segment_count);
//			printf("string=\"%s\" maybe not zipped.\n", text.c_str());
//#endif
			temp.dump_save_segment(output, header);
			temp.clear();
			if (1 == equal_count)
				temp.push_back(text, *ilow);
			else
				temp.push_back(text, ilow, iupp, equal_count);

			segment_count++;
		}
	}
	if (!temp.empty())
		temp.dump_save_segment(output, header), segment_count++;

	printf("%d segments saved.\n", segment_count);
}

template<class SrcTableT, class Output>
void dump_save_to_merged_aux(const SrcTableT& src, Output& output, boost::mpl::false_ isSeekable)
{
	typename SrcTableT::fixed_index::dump_header header(src);
	dump_save_to_merged_impl(src, output, header);
}

template<class SrcTableT, class Output>
void dump_save_to_merged_aux(const SrcTableT& src, Output& output, boost::mpl::true_ isSeekable)
{
	typename SrcTableT::fixed_index::dump_header header(src);

	stream_position_t start_pos = output.getStream()->tell();
	dump_save_to_merged_impl(src, output, header);

	//
	// update header, this will lead more efficient when load from the stream
	//
	stream_position_t curr_pos = output.getStream()->tell();
	output.getStream()->seek(start_pos); // seek to header's position
	output << header; // update header
	output.getStream()->seek(curr_pos); // restore file position
}

template<class SrcTableT, class Output>
void dump_save_to_merged(const SrcTableT& src, Output& output)
{
	dump_save_to_merged_aux(src, output, typename Output::is_seekable());
}

//////////////////////////////////////////////////////////////////////////

template<class Input, class TableT>
void dump_load_merged(TableT& table, Input& input)
{
	typename TableT::dump_header loading, loaded;
	input >> loaded;

	table.prepair_dump_load_merged(loaded);

	int segment_count = 0;
	while (loading.less_than(loaded))
	{
		table.dump_load_segment(input, loading, loaded);
		segment_count++;
	}
	table.complete_dump_load_merged(input, loading, loaded);
	printf("%d segments loaded.\n", segment_count);
}

/*
template<class TableT>
class DumpMergeLoadProxy
{
	TableT& tab;
public:
	explicit DumpMergeLoadProxy(TableT& tab) : tab(tab) { }
	template<class DataIO> void load(DataIO& ar) { dump_load_merged(tab, ar); }
	template<class DataIO> void save(DataIO& ar) const { TableT::ThisClassCanNotSaveThisWay(ar); }
	DATA_IO_REG_LOAD_SAVE(DumpMergeLoadProxy)
};
template<class MultiWayTableT, class ValueCompare>
class DumpMergeSaveProxy
{
	MultiWayTableT& tab;
	ValueCompare    vcomp;
public:
	DumpMergeSaveProxy(MultiWayTableT& tab, ValueCompare vcomp) : tab(tab), vcomp(vcomp) { }
	template<class DataIO> void load(DataIO& ar) { MultiWayTableT::ThisClassCanNotLoadThisWay(ar); }
	template<class DataIO> void save(DataIO& ar) const { dump_save_to_merged(tab, ar, vcomp); }
	DATA_IO_REG_LOAD_SAVE(DumpMergeSaveProxy)
};

//-**************************************************************************************
//! @{
//! use as below:
//! @code
//!   MyMultiWayTable mwtab;
//!   MyMultiWayTable::fixed_index tab;
//!   native_save_to_file(DumpMergeSave(mwtab, vcomp), "file.merged.dump"); // use vcomp
//!   native_save_to_file(DumpMergeSave(mwtab), "file.merged.dump"); // use default vcomp=std::less<value_type>
//!   native_load_from_file(DumpMergeLoad(tab), "file.merged.dump");
//! @endcode
//-**************************************************************************************
template<class TableT>
pass_by_value<DumpMergeLoadProxy<TableT> >
DumpMergeLoad(TableT& tab)
{
	return pass_by_value<DumpMergeLoadProxy<TableT> >(DumpMergeLoadProxy<TableT>(tab));
}
//! DumpMergeSave not need pass_by_value, temp object can bound to const reference
template<class MultiWayTableT, class ValueCompare>
DumpMergeSaveProxy<MultiWayTableT, ValueCompare>
DumpMergeSave(MultiWayTableT& tab, ValueCompare vcomp)
{
	return DumpMergeSaveProxy<MultiWayTableT, ValueCompare>(tab);
}
template<class MultiWayTableT>
DumpMergeSaveProxy<MultiWayTableT, std::less<typename MultiWayTableT::value_type> >
DumpMergeSave(MultiWayTableT& tab)
{
	typedef std::less<typename MultiWayTableT::value_type> def_vcomp;
	return DumpMergeSaveProxy<MultiWayTableT, def_vcomp>(tab, def_vcomp());
}
//@}
*/

template<class TableT, class Output>
void table_save_kvset(const TableT& itab, Output& output)
{
	using namespace std;

	uint32_t key_count = itab.key_count();
	uint32_t val_count = itab.val_count();
	uint32_t zip_size  = itab.zip_key_size() + itab.incr_key_size();

	output << key_count << val_count << zip_size;

	uint32_t part = max(uint32_t(100), uint32_t(key_count/1000));
	uint32_t val_count2 = 0; // compute actural val_count2 when save
	uint32_t nth_key = 0;
	typename TableT::const_iterator iend = itab.end();
	for (typename TableT::const_iterator iter = itab.begin(); iter != iend; ++nth_key)
	{
		typename TableT::const_iterator ilow = iter;
		uint32_t count = itab.goto_next_larger(iter);
		output << itab.key(ilow);
		std::vector<typename TableT::value_type> vals;
		vals.reserve(count);
		itab.query_result(ilow, vals); // more fast
	//	vals.insert(vals.end(), ilow, iter);
		assert(vals.size() == count);
		output << vals;
//		DataIO_dump_save_object(vals, output);
		val_count2 += vals.size();

		if (nth_key % part == 1)
			printf("."), fflush(stdout);
	}
	printf("\nval_count[all=%lu, actual=%lu]\n", val_count, val_count2);
}

/**
 @brief 存储时删除重复的 values
 */
template<class TableT, class Output, class ValueCompare>
void table_save_kvset_unique_val(const TableT& itab, Output& output, ValueCompare vcomp)
{
	using namespace std;

	uint32_t key_count = itab.key_count();
	uint32_t val_count = itab.val_count();
	uint32_t zip_size  = itab.zip_key_size() + itab.incr_key_size();

	stream_position_t start_pos = output.getStream()->tell();

	output << key_count << val_count << zip_size;

	uint32_t part = max(uint32_t(100), uint32_t(val_count/TERARK_IF_DEBUG(20000, 1000)));
	uint32_t val_count2 = 0; // compute actual val_count2 when save
	uint32_t nth_key = 0;
	typename TableT::const_iterator iend = itab.end();
	for (typename TableT::const_iterator iter = itab.begin(); iter != iend; ++nth_key)
	{
		typename TableT::const_iterator ilow = iter;
		uint32_t count1 = itab.goto_next_larger(iter);
		output << itab.key(ilow);
		std::vector<typename TableT::value_type> vals;
		vals.reserve(count1);
		itab.query_result(ilow, vals); // more fast
	//	vals.insert(vals.end(), ilow, iter);
		assert(vals.size() == count1);
	//	uint32_t count2 = vals.size();
		std::sort(vals.begin(), vals.end(), vcomp);
		vals.erase(terark::set_unique(vals.begin(), vals.end(), terark::not2(vcomp)),
				   vals.end());
		output << vals;
	//	DataIO_dump_save_object(vals, output);
		val_count2 += vals.size();

 		if (nth_key % part == 1)
 			printf("."), fflush(stdout);

	//	if (count1 != vals.size())
	//		printf("count[1=%lu, 2=%lu, 3=%lu]\n", count1, count2, vals.size());
	}
	stream_position_t curr_pos = output.getStream()->tell();
	output.getStream()->seek(start_pos);
	output << key_count << val_count2;
	output.getStream()->seek(curr_pos);

	printf("\nval_count[all=%lu, unique=%lu]\n", val_count, val_count2);
//	printf("key_count[1=%lu, 2=%lu]\n", key_count, nth_key);
}

template<class TableT, class Input>
void table_load_kvset(TableT& itab, Input& input)
{
	uint32_t key_count, val_count, zip_size;

	input >> key_count >> val_count >> zip_size;
	itab.clear();
	itab.init(zip_size);
//	itab.reserve(val_count, key_count);
	itab.reserve(val_count);
	for (uint32_t i = 0; i < key_count; ++i)
	{
		typename TableT::key_type xkey;
		std::vector<typename TableT::value_type> vals;
		input >> xkey;
		input >> vals;
	//	DataIO_dump_load_object(vals, output);
		itab.push_back(xkey, vals.begin(), vals.end(), vals.size());
	}
}

//! 按 (key, value_set) 存储
//! @{
#define TERARK_LOAD_FUNCTION_NAME    native_load_kvset_from_file
#define TERARK_SAVE_FUNCTION_NAME    native_save_kvset_to_file
#define TERARK_DATA_INPUT_CLASS      LittleEndianDataInput
#define TERARK_DATA_OUTPUT_CLASS     LittleEndianDataOutput
#define TERARK_DATA_INPUT_LOAD_FROM(input, x)  table_load_kvset(x, input)
#define TERARK_DATA_OUTPUT_SAVE_TO(output, x)  table_save_kvset(x, output)
#include <terark/io/load_save_convenient.h>

#define TERARK_LOAD_FUNCTION_NAME    portable_load_kvset_from_file
#define TERARK_SAVE_FUNCTION_NAME    portable_save_kvset_to_file
#define TERARK_DATA_INPUT_CLASS      PortableDataInput
#define TERARK_DATA_OUTPUT_CLASS     PortableDataOutput
#define TERARK_DATA_INPUT_LOAD_FROM(input, x)  table_load_kvset(x, input)
#define TERARK_DATA_OUTPUT_SAVE_TO(output, x)  table_save_kvset(x, output)
#include <terark/io/load_save_convenient.h>
//@}

//! 按 (key, value_set) 存储，同时删除相同 key 的重复的 value
//! @{
#define TERARK_SAVE_FUNCTION_NAME    native_save_kvset_to_file
#define TERARK_DATA_OUTPUT_CLASS     LittleEndianDataOutput
#define TERARK_DATA_IO_LS_EX_T_PARAM , class ValueCompare
#define TERARK_DATA_IO_LS_EX_PARAM	 , ValueCompare vcomp
#define TERARK_DATA_OUTPUT_SAVE_TO(output, x)  table_save_kvset_unique_val(x, output, vcomp)
#include <terark/io/load_save_convenient.h>

#define TERARK_SAVE_FUNCTION_NAME    portable_save_kvset_to_file
#define TERARK_DATA_OUTPUT_CLASS     PortableDataOutput
#define TERARK_DATA_IO_LS_EX_T_PARAM , class ValueCompare
#define TERARK_DATA_IO_LS_EX_PARAM	 , ValueCompare vcomp
#define TERARK_DATA_OUTPUT_SAVE_TO(output, x)  table_save_kvset_unique_val(x, output, vcomp)
#include <terark/io/load_save_convenient.h>
//@}

//! @{
#define TERARK_LOAD_FUNCTION_NAME    dump_load_merged_from_file
#define TERARK_SAVE_FUNCTION_NAME    dump_save_merged_to_file
#define TERARK_DATA_INPUT_CLASS      LittleEndianDataInput
#define TERARK_DATA_OUTPUT_CLASS     LittleEndianDataOutput
#define TERARK_DATA_INPUT_LOAD_FROM(input, x)  dump_load_merged(x, input)
#define TERARK_DATA_OUTPUT_SAVE_TO(output, x)  dump_save_to_merged(x, output)
#include <terark/io/load_save_convenient.h>
//@}

//! @{
#define TERARK_SAVE_FUNCTION_NAME    dump_save_merged_to_file
#define TERARK_DATA_OUTPUT_CLASS     LittleEndianDataOutput
#define TERARK_DATA_IO_LS_EX_T_PARAM , class ValueCompare
#define TERARK_DATA_IO_LS_EX_PARAM	 , ValueCompare vcomp
#define TERARK_DATA_OUTPUT_SAVE_TO(output, x)  dump_save_to_merged(x, output, vcomp)
#include <terark/io/load_save_convenient.h>
//@}


} } // namespace terark::prefix_zip

#endif // __terark_serialize_common_h__


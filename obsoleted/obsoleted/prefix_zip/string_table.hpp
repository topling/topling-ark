/* vim: set tabstop=4 : */
/********************************************************************
	@file string_table.hpp
	@brief 综合 ZStringTable 和 PackedTable 各自优点的 MultiWayTable_Impl

	@date	2006-9-29 11:06
	@author	Lei Peng
	@{
*********************************************************************/
#ifndef __terark_string_table_h__
#define __terark_string_table_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
# pragma warning(push)
# pragma warning(disable: 4018) // '<' : signed/unsigned mismatch
#endif

#include "multi_zstring_table.hpp"
#include "multi_way_table.hpp"

namespace terark { namespace prefix_zip {

template<uint_t FlagLength>
struct StringTable_dump_header : public MultiWayTable_Impl_dump_header
{
	uint32_t m_flagLength;

	StringTable_dump_header()
	{
		this->m_flagLength = FlagLength;
	}
	template<class SrcTable>
	explicit StringTable_dump_header(const SrcTable& src)
		: MultiWayTable_Impl_dump_header(src)
	{
		BOOST_STATIC_ASSERT(FlagLength == SrcTable::FLAG_LENGTH);
		m_flagLength = SrcTable::FLAG_LENGTH;
	}
	void check() const { check_flag_length(this->m_flagLength, FlagLength); }

	DATA_IO_REG_LOAD_SAVE_V(StringTable_dump_header, 1,
		& vmg.template base<MultiWayTable_Impl_dump_header>(this)
		& m_flagLength
		)
};

template<class ValueT, class CompareT, uint_t FlagLength, class KeyCategory>
class StringTable_Base
{
	BOOST_STATIC_ASSERT(2 <= FlagLength && FlagLength <= 4);

public:
	BOOST_STATIC_CONSTANT(bool, IsUniqueKey = (boost::is_same<KeyCategory, UniqueKey>::value));

protected:
	unzip_prefer m_unzip_prefer;

	BOOST_STATIC_CONSTANT(bool, IsBiWay = (boost::is_same<BiWayTableKey  , KeyCategory>::value));
	BOOST_STATIC_CONSTANT(bool, IsIndex = (boost::is_same<MultiIndexedKey, KeyCategory>::value));

	typedef ZStringTable<ValueT, CompareT, FlagLength, IsUniqueKey> base_fixed_t;
	typedef PackedTable<std::string, ValueT, CompareT, IsUniqueKey>	base_increment_t;

	typedef MultiSortedTable<base_fixed_t>              indexed_fixed_t;
	typedef MultiPackedTable<std::string, ValueT, CompareT> indexed_increment_t;

public:
	BOOST_STATIC_CONSTANT(uint_t, FLAG_LENGTH = FlagLength);
	typedef KeyCategory key_category;

	typedef typename boost::mpl::if_c<
				IsBiWay, MultiZStringTable<ValueT, CompareT, FlagLength>,
				typename boost::mpl::if_c<
					IsIndex, indexed_fixed_t, base_fixed_t
				>::type
			>::type fixed_index;
	typedef std::vector<fixed_index> fixed_index_vec;

	/**
	 @brief 仅保存 string 部分，无对应的 value

	 用于将 table 中的 string 导出
	 */
	typedef ZStringTable<nil, CompareT, FlagLength, true> zstr_set;

	typedef typename boost::mpl::if_c<
				IsBiWay, BiWayTable<indexed_increment_t, base_increment_t>,
				typename boost::mpl::if_c<
					IsIndex, indexed_increment_t, base_increment_t
				>::type
			>::type increment_index;

	typedef CompareT    compare_t;
	typedef std::string	key_type;
	typedef ValueT		value_type;
	typedef ValueT&		reference;

	typedef StringTable_dump_header<FlagLength> dump_header;

	BOOST_STATIC_CONSTANT(uint_t, MAX_ZSTRING = ZString<FlagLength>::MAX_ZSTRING);

	StringTable_Base()
	{
		m_unzip_prefer = boost::is_same<KeyCategory, BiWayTableKey>::value ? uzp_small_dup : uzp_middle_dup;
	}

	void set_unzip_prefer(fixed_index_vec& fixed, unzip_prefer uzp)
	{
		for (typename fixed_index_vec::iterator i = fixed.begin(); i != fixed.end(); ++i)
		{
			(*i).set_unzip_prefer(uzp);
		}
		m_unzip_prefer = uzp;
	}
	void set_unzip_prefer(fixed_index& fixed)
	{
		fixed.set_unzip_prefer(m_unzip_prefer);
	}
};

template<class ValueT, class CompareT=StringCompareSZ, uint_t FlagLength=3, class KeyCategory=UniqueKey>
class StringTable : public MultiWayTable_Impl<StringTable_Base<ValueT, CompareT, FlagLength, KeyCategory> >
{
	typedef MultiWayTable_Impl<StringTable_Base<ValueT, CompareT, FlagLength, KeyCategory> > super;

public:
	typedef typename super::size_type size_type;

	explicit StringTable(uint32_t maxPoolSize = TERARK_IF_DEBUG(8*1024, 2*1024*1024),
						 CompareT comp = CompareT(),
						 size_type initialFixedIndexCapacity = 512)
		: super(maxPoolSize, comp, initialFixedIndexCapacity)
	{
	}
	DATA_IO_REG_DUMP_LOAD_SAVE(StringTable)
	DATA_IO_REG_LOAD_SAVE_V(StringTable, 1)
};

#define TERARK_ZSTR_SET(CompareT, FlagLength) \
	terark::prefix_zip::StringTable<terark::prefix_zip::nil, CompareT, FlagLength, terark::prefix_zip::UniqueKey>

} } // namespace terark::prefix_zip

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma warning(pop)
#endif

#endif // __terark_string_table_h__

// @} end file string_table.hpp

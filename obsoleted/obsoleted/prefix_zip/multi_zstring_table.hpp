/* vim: set tabstop=4 : */
#ifndef __terark_multi_zstring_table_h__
#define __terark_multi_zstring_table_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#define TERARK_NEW_ZSTRING_TABLE

#ifdef TERARK_NEW_ZSTRING_TABLE

#include <terark/prefix_zip/serialize_common.hpp>

#include "zstring_table.hpp"
#include "multi_sorted_table.hpp"
#include "biway_table.hpp"

namespace terark { namespace prefix_zip {

template<class ValueT, class CompareT, uint_t FlagLength>
class MultiZStringTable : public
	BiWayTable<
		MultiSortedTable<ZStringTable<ValueT, CompareT, FlagLength, false> >,
		ZStringTable<ValueT, CompareT, FlagLength, false>
	>
{
	typedef
	BiWayTable<
		MultiSortedTable<ZStringTable<ValueT, CompareT, FlagLength, false> >,
		ZStringTable<ValueT, CompareT, FlagLength, false>
	> super;

public:
	BOOST_STATIC_CONSTANT(uint_t, FLAG_LENGTH = FlagLength);
	BOOST_STATIC_CONSTANT(uint_t, MAX_ZSTRING = ZString<FlagLength>::MAX_ZSTRING);

	MultiZStringTable(uint32_t maxPoolSize = TERARK_IF_DEBUG(8*1024, 2*1024*1024),
					  CompareT comp = CompareT())
		: super(maxPoolSize, comp) {}
};

} } // namespace terark::prefix_zip

#else

#include "multi_zstring_table_old.h"

#endif


#endif // __terark_multi_zstring_table_h__


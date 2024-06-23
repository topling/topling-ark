/* vim: set tabstop=4 : */
#ifndef __terark_dump_header_h__
#define __terark_dump_header_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#include <terark/io/DataIO.hpp>

namespace terark { namespace prefix_zip {

struct Table_dump_header
{
	uint32_t version;
	uint32_t keyCount; // keyCount maybe same with valCount
	uint32_t valCount;

	template<class SrcTable>
	explicit Table_dump_header(const SrcTable& src)
	{
		version  = 1;
		keyCount = src.key_count();
		valCount = src.val_count();
	//	valCount = src.size();
	}
	Table_dump_header()
	{
		version  = 1;
		keyCount = 0;
		valCount = 0;
	}

	bool less_than(const Table_dump_header& loaded) const
	{
		return valCount < loaded.valCount;
	}
	void check() const { }

	DATA_IO_REG_LOAD_SAVE_V(Table_dump_header, 1,
		& keyCount
		& valCount
		& vmg.get_version(version)
		)
};


} } // namespace terark::prefix_zip


#endif // __terark_dump_header_h__


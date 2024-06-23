#ifndef __terark_bdb_util_file_space_h__
#define __terark_bdb_util_file_space_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#if defined(_WIN32) || defined(_WIN64)
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif
#endif

#include "../stdtypes.hpp"
#include <boost/function.hpp>

namespace terark {

template<class PositionT, class SizeT>
class SpaceManager_Impl;

/**
 * @deprecated:
 */
template<class PositionT, class SizeT>
class SpaceManager
{
	SpaceManager_Impl<PositionT, SizeT>* m_impl;

public:
	typedef PositionT position_type;
	typedef SizeT     size_type;
	typedef std::pair<PositionT, SizeT> pos_size_t;

	//! prototype is: pos_size_t make_new_space(SizeT min_space)
	typedef boost::function1<pos_size_t, SizeT> make_new_space_ft;

	explicit SpaceManager(SizeT cell_size,
						  SizeT init_space_size,
						  const make_new_space_ft& make_new_space);

	explicit SpaceManager(const make_new_space_ft& make_new_space);

	~SpaceManager();

	PositionT alloc(SizeT size);

	PositionT alloc(PositionT hint_pos,
					SizeT  old_size,
					SizeT  min_size,
					SizeT* max_size);

	void free(PositionT pos, SizeT size);
};

typedef SpaceManager<stream_position_t, stream_position_t> FileSpace;
//typedef SpaceManager<unsigned char*, unsigned int> MemSpace;

} // namespace terark


#endif // __terark_bdb_util_file_space_h__


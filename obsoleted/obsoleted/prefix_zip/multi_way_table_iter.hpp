/* vim: set tabstop=4 : */

#define MULTI_WAY_EXTRACT_KV(tab, iter, kv)  tab.key(iter, kv)

// #define MULTI_WAY_EXTRACT_KV(tab, iter, kv)  *iter

//////////////////////////////////////////////////////////////////////////
//! generate iterator/const_iterator
#define TERARK_iter_beg begin
#define TERARK_iter_end end

#define TERARK_MWT_ITER_compare_t			typename BaseClassT::compare_t

#define TERARK_M_const const
#define TERARK_M_iterator const_iterator
#define TERARK_M_reverse_iterator const_reverse_iterator

#define TERARK_MWT_ITER_is_const_iterator
#include "multi_way_table_iter_i.hpp"
#undef  TERARK_MWT_ITER_is_const_iterator

#define TERARK_M_const
#define TERARK_M_iterator iterator
#define TERARK_M_reverse_iterator reverse_iterator
#include "multi_way_table_iter_i.hpp"

#undef TERARK_iter_beg
#undef TERARK_iter_end
#undef TERARK_MWT_ITER_compare_t
//////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////
//! generate reverse_iterator/const_reverse_iterator
#define TERARK_MWT_ITER_is_reverse_iterator

#define TERARK_iter_beg rbegin
#define TERARK_iter_end rend
#define TERARK_MWT_ITER_compare_t InverseCompare<typename BaseClassT::compare_t>

#define TERARK_M_const const
#define TERARK_M_iterator const_reverse_iterator
#define TERARK_M_reverse_iterator const_reverse_iterator

#define TERARK_MWT_ITER_is_const_iterator
#include "multi_way_table_iter_i.hpp"
#undef  TERARK_MWT_ITER_is_const_iterator

#define TERARK_M_const
#define TERARK_M_iterator reverse_iterator
#define TERARK_M_reverse_iterator reverse_iterator
#include "multi_way_table_iter_i.hpp"

#undef TERARK_iter_beg
#undef TERARK_iter_end
#undef TERARK_MWT_ITER_compare_t

#undef TERARK_MWT_ITER_is_reverse_iterator
//////////////////////////////////////////////////////////////////////////


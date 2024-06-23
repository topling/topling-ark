#ifndef __terark_word_table
#define __terark_word_table

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

// TODO: Î´Íê³É

#include <vector>
#include <boost/type_traits.hpp>

#include "../prefix_zip/key_pool.hpp"
#include "const_string.hpp"

// should be the last #include
//#include <boost/type_traits/detail/bool_trait_def.hpp>

namespace terark {

template<class StringT>
class TextExtractor
{
public:
	typedef basic_const_string<boost::remove_const<typename StringT::value_type>::type> const_string;

	explicit TextExtractor(const StringT* str)
	{
		m_str = str;
	}

	const_string operator()(size_t offset_beg, size_t offset_end) const
	{
		return const_string(m_str->begin() + offset_beg, m_str->begin() + offset_end);
	}

private:
	const StringT* m_str;
};

template<class

} // namespace terark

#endif // __terark_word_table


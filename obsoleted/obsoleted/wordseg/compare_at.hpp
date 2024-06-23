#ifndef __terark_wordseg_compare_at_h__
#define __terark_wordseg_compare_at_h__

#include <assert.h>

namespace terark { namespace wordseg {

	template<class CharT>
	struct ChKey
	{
		CharT c;

		explicit ChKey(CharT c) : c(c) {}
	};

	// 使用非对称 Compare, 可以减少一个间接 Char 访问
	template<class CharT>
	struct CompareAt
	{
		size_t pos;
		size_t len;
		const CharT *str;

		bool operator()(ChKey<CharT> x, size_t y) const
		{
			assert(y + pos >= 0);
			assert(y + pos < len);
			return x.c < str[y + pos];
		}
		bool operator()(size_t x, ChKey<CharT> y) const
		{
			assert(x + pos >= 0);
			assert(x + pos < len);
			return str[x + pos] < y.c;
		}

		// 对称 Compare, for sort
		bool operator()(size_t x, size_t y) const
	   	{
			assert(x + pos < len);
			assert(y + pos < len);
		//	assert(str[x + pos] > 0);
		//	assert(str[y + pos] > 0);
		   	return str[x + pos] < str[y + pos];
	   	}
	};

} } // namespace terark::wordseg

#endif // __terark_wordseg_compare_at_h__


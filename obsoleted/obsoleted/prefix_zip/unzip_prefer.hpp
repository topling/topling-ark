/* vim: set tabstop=4 : */
#ifndef __terark_unzip_prefer_h__
#define __terark_unzip_prefer_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif


namespace terark { namespace prefix_zip {

/**
 @brief 根据相同 key 对应的不同 value 数目大小的不同，使用不同的算法

 在字符串解压时需要通过搜索来确定前缀和字符串尺寸
 - 在重复数目较小时，使用线性搜索更快，
 - 较大时，使用 near bin search 较快
 - 而很大时，使用传统的 bin search 更快
 */
enum unzip_prefer
{
	uzp_large_dup,
	uzp_small_dup,
	uzp_middle_dup
};

} } // namespace terark::prefix_zip


#endif // __terark_unzip_prefer_h__


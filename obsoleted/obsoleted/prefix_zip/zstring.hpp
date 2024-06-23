/* vim: set tabstop=4 : */
#ifndef __zstring_h__
#define __zstring_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#include <terark/stdtypes.hpp>
#include <terark/io/InputStream.h>
#include <terark/io/DataIO.hpp>
#include <boost/config.hpp>

namespace terark { namespace prefix_zip {

/**
 @addtogroup prefix_zip
 @{
*/

/**
 @brief ZString 的声明【没有“定义”一般FlagLength的ZString<FlagLength>】

  - 目前仅实现了三种 ZString, 还看不出需要其它 ZString 的需求。\n
	- ZString<2>\n
	  - MAX_ZSTRING =  66 -- 最大的字符串长度必须小于等于 66\n
      - MAX_DIFF    = 132 -- 最大前缀距离是 132，也就是说：\n
			- 如果有多于 133 个 string 有相同的前缀，\n
			- 排在后面的字符串解压时间就会成倍增加\n
			- 如果有 134 个 "dir1/dir2/file-*******" (*表示一个任意字符)\n
			- 解压第 134 个 string 时，解压时间就会是第 133 个的大约 2 倍\n

	- ZString<3>
      - MAX_ZSTRING = 259
      - MAX_DIFF    = 33 * 256 (8448)
	- ZString<4>
      - MAX_ZSTRING = 65535
      - MAX_DIFF    = 33 * 256 (8448) same as ZString<3>

 - 模板参数 FlagLength 当 string 是压缩的 string 时，\n
			有 FlagLength 个字节用来保存压缩头信息，其后是后缀。\n
			FlagLength 越小，压缩率越大，但能保存的最大 string 长度就较小，\n
			并且解压时间可能较大.

 @see ZString<2>, ZString<3>, ZString<4>
*/
#ifdef __DOXYGEN_PARSER
	template<uint_t FlagLength> class ZString {}; // definition
#else
	template<uint_t FlagLength> class ZString;    // declaration
#endif
//@}

/**
 @brief 转义输入的字符

 @param diff 被转义的字符
 @return 如果 diff <= 32，返回 diff 的值，否则返回 -1
 @note 这个实现可能会被改动
 @note 这是个内部函数，用户不应调用它
 */
int ZString_EscapeByte(byte diff);

/**
 @brief ZString<3> 和 ZString<4> 的数据表示

  ZString<3> 和 ZString<4> 的不同之处仅在于 nPrefix 的类型 PrefixLengthType 不同\n
  ZString<3>::nPrefix 类型为 byte
  ZString<4>::nPrefix 类型为 uint_16
  这个不同之处使得它们可以包含的最大字符串长度有很大差别 @see BaseZString_3, BaseZString_4

  @note 仅内部实现用，不可由用户代码使用
 */
template<class PrefixLengthType>
class BaseZString_3_4
{
protected:
	union {
		struct {
			// diff1 <= 0x20 means this string has prefix,
			// otherwise, this is complete string...
			// if this ZString has prefix
			//   prefix ZString is index[current - diff1 * 256 + u.diff2],
			//   and prefix length is nPrefix
			byte diff1;
			byte diff2;

			// 有这么多个字符是引用第 (前 diff) 个 ZString 的
			// 被引用的 ZString 也可以引用更前面的字符
			PrefixLengthType nPrefix;

			char suffix[1];
		}u;
		char m_text[3 + sizeof(PrefixLengthType)];
	};
	// some data maybe follow here
};

class BaseZString_3 : public BaseZString_3_4<byte>
{
public:
	BOOST_STATIC_CONSTANT(uint_t, FLAG_LENGTH = 3);
	BOOST_STATIC_CONSTANT(uint_t, MAX_ZSTRING = FLAG_LENGTH + 255 + 1); // 259
};

class BaseZString_4 : public BaseZString_3_4<uint16_t>
{
public:
	BOOST_STATIC_CONSTANT(uint_t, FLAG_LENGTH = 4);
	BOOST_STATIC_CONSTANT(uint_t, MAX_ZSTRING = 65535);
};

template<class BaseZStringT>
class ZStringImpl_3_4 : public BaseZStringT
{
	typedef BaseZStringT super;

public:
	BOOST_STATIC_CONSTANT(uint_t, MAX_DIFF = 33 * 256); // 8848

	uint_t prefixLength() const
	{
		assert(getDiff() != 0);
		return super::u.nPrefix + super::FLAG_LENGTH + 1;
	}
	void setPrefixLength(uint_t nPrefix)
	{
		assert(super::FLAG_LENGTH < nPrefix && nPrefix <= super::MAX_ZSTRING);
		super::u.nPrefix = nPrefix - super::FLAG_LENGTH - 1;
	}

	// always >= 1
	uint_t getDiff() const
	{
		int diff1 = ZString_EscapeByte(super::u.diff1);
		if (-1 == diff1) return 0;

		uint_t diff = diff1 * 256 + super::u.diff2;
		assert(diff < MAX_DIFF);

		return diff + 1;
	}
	void setDiff(uint_t diff)
	{
		assert(diff >= 1 && diff <= MAX_DIFF);

		diff -= 1;

		super::u.diff1 = diff / 256;
		super::u.diff2 = diff % 256;
	}

	bool isFirstByteEscape() const
	{
		return (0xFF == byte(this->m_text[0]));
	}
	char* text()
	{
		if (0xFF == byte(this->m_text[0]))
			return this->m_text + 1;
		else
			return this->m_text;
	}
	char* suffix()
	{
		return super::u.suffix;
	}
};

template<> class ZString<3> : public ZStringImpl_3_4<BaseZString_3> { };
template<> class ZString<4> : public ZStringImpl_3_4<BaseZString_4> { };

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
template<> class ZString<2>
{
public:
	BOOST_STATIC_CONSTANT(uint_t, FLAG_LENGTH = 2);
	BOOST_STATIC_CONSTANT(uint_t, MAX_ZSTRING = FLAG_LENGTH + 1 + 63); // 66
	BOOST_STATIC_CONSTANT(uint_t, MAX_DIFF	= 33 * 4); // 132

	uint_t prefixLength() const;
	void setPrefixLength(uint_t nPrefix);

	// always >= 1
	uint_t getDiff() const;
	void setDiff(uint_t diff);

	char* text();
	char* suffix();

	bool isFirstByteEscape() const;

private:
	union {
		struct {
			// b1 <= 0x20 means this string has prefix, and
			// otherwise, this is complete string...
			// if this ZString has prefix
			//   prefix ZString is index[current - getDiff()],
			//   and prefix length is nPrefix
			byte b1;
			byte b2; // high 2bits is low 2bits of diff, low 6bits is prefix length

			// 有这么多个字符是引用第 (前 b1) 个 ZString 的
			// 被引用的 ZString 也可以引用更前面的字符

			char suffix[1];
		}u;
		char m_text[3];
	};
	// some data maybe follow here
};

//////////////////////////////////////////////////////////////////////////
inline uint_t ZString<2>::prefixLength() const
{
	assert(getDiff() != 0);
	return  FLAG_LENGTH + 1 + (u.b2 & 63);
}

inline void ZString<2>::setPrefixLength(uint_t nPrefix)
{
	assert(FLAG_LENGTH < nPrefix && nPrefix <= MAX_ZSTRING);
	u.b2 = (u.b2 & 0xC0) | nPrefix - FLAG_LENGTH - 1;
}

inline uint_t ZString<2>::getDiff() const
{
	int diff_high = ZString_EscapeByte(u.b1);
	if (-1 == diff_high) return 0;

	uint_t diff = diff_high * 4 + (u.b2 >> 6);
	assert(diff < MAX_DIFF);

	return diff + 1;
}

inline void ZString<2>::setDiff(uint_t diff)
{
	assert(diff >= 1 && diff <= MAX_DIFF);

	diff -= 1;

	u.b1 = diff / 4;
	u.b2 = u.b2 & 63 | diff % 4 << 6;
}

inline char* ZString<2>::text()
{
//	assert(getDiff() == 0);
	if (0xFF == u.b1)
		return m_text + 1;
	else
		return m_text;
}

inline bool ZString<2>::isFirstByteEscape() const
{
	return (0xFF == u.b1);
}

inline char* ZString<2>::suffix()
{
//	assert(getDiff() != 0);
	return u.suffix;
}

//////////////////////////////////////////////////////////////////////////

} } // namespace terark::prefix_zip


#endif // __zstring_h__


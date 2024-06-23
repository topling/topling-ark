#include "packed_table.hpp"

namespace terark { namespace prefix_zip {

///////////////////////////////////////////////////////////////////////////////

uint32_t PackedStringPool::put(const std::string& str)
{
	return put(str.c_str(), str.size());
}

uint32_t PackedStringPool::put(const char* szstr)
{
	return put(szstr, strlen(szstr));
}

uint32_t PackedStringPool::put(const char* szstr, int nstr)
{
	uint32_t offset = this->tell();
	if (offset + nstr + 1 > this->size())
	{
		char szMsg[10240];
		#if defined(_WIN32) || defined(_WIN64)
			_snprintf(szMsg, sizeof(szMsg),
		#else
			snprintf(szMsg, sizeof(szMsg),
		#endif
			"PackedStringPool::put(szstr=%s, nstr=%u)", szstr, nstr);
		throw OutOfSpaceException(szMsg);
	}
	this->write(szstr, nstr);
	this->writeByte(0); // null end
	return offset;
}

const char* PackedStringPool::c_str(uint32_t offset) const
{
	assert(offset >= 0 && offset < this->tell());
	return (const char*)(this->buf() + offset);
}

///////////////////////////////////////////////////////////////////////////////

} } // namespace terark::prefix_zip

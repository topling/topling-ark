/* vim: set tabstop=4 : */
#include "string_pool.hpp"

namespace terark { namespace prefix_zip {

///////////////////////////////////////////////////////////////////////////////

uint32_t StringPool::make_key(const std::string& str)
{
	return make_key(str.c_str(), str.size());
}

uint32_t StringPool::make_key(const char* szstr)
{
	return make_key(szstr, strlen(szstr));
}

uint32_t StringPool::make_key(const char* szstr, int nstr)
{
	uint32_t offset = this->tell();
	if (offset + nstr + 1 > this->size())
	{
		string_appender<> oss;
		oss << "StringPool::make_key"
			<< "(szstr=" << szstr
			<< ", nstr=" << nstr
			<< ", offset=" << offset
			<< ", pool_size=" << AutoGrownMemIO::size()
			<< ")";
		throw OutOfSpaceException(oss.str().c_str());
	}
	this->write(szstr, nstr);
	this->writeByte(0); // null end
	m_key_count++;
	return offset;
}

void StringPool::unmake_key(const std::string& key)
{
	this->seek(this->tell() - key.size() - 1);
	m_key_count--;
}

///////////////////////////////////////////////////////////////////////////////

} } // namespace terark::prefix_zip

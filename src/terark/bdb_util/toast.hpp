/* vim: set tabstop=4 : */
#ifndef __terark_bdb_utility_toast_h__
#define __terark_bdb_utility_toast_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#if defined(_WIN32) || defined(_WIN64)
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif
#endif

#include <terark/stdtypes.hpp>
#include <string>

class DbEnv; // forward declare

namespace terark {

class TERARK_DLL_EXPORT ToastBlob
{
	DECLARE_NONE_COPYABLE_CLASS(ToastBlob)
public:
	ToastBlob(DbEnv* env,
			const std::string& prefix,
			const std::string& toastFile,
			uint32_t cell_size = 16,
			uint32_t initial_size = 0);

	virtual ~ToastBlob();

	stream_position_t insert(const void* data, uint32_t size);
	stream_position_t update(stream_position_t old_pos, uint32_t old_size, const void* data, uint32_t* size);
	stream_position_t append(stream_position_t old_pos, uint32_t old_size, const void* data, uint32_t size);

	void query(stream_position_t pos, void* data, uint32_t size);

protected:
	class ToastBlob_impl* impl;
};

} // namespace terark


#endif // __terark_bdb_utility_toast_h__

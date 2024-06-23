/* vim: set tabstop=4 : */
#ifndef __terark_SynchObject_h__
#define __terark_SynchObject_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#include <stdexcept>
#include "../thread/Exception.hpp"
#include <terark/stdtypes.hpp>
#include "LockSentry.hpp"

namespace terark { namespace thread {

/**
 @addtogroup thread
 @{
 */

/**
 @defgroup SynchObject 线程同步对象
 @{
  - 这些对象的实现是平台相关的，平台相关的头文件
	- /terark/thread/posix
	- /terark/thread/win32
 @}
 */
//@}

} } // namespace terark::thread

#if defined(_WIN32) || defined(WIN32)
#	include "win32/SynchObject.hpp"
#else
#	include "posix/SynchObject.hpp"
#endif

#endif // __terark_SynchObject_h__


/* vim: set tabstop=4 : */
// file: terark/thread/win32/thread_impl.hpp

#ifndef __terark_thread_h__
#	error "can not include this file by user code!!"
#endif

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "../thread_base.hpp"

namespace terark { namespace thread {

/**
 @addtogroup ThreadObject
 @{
 @defgroup ThreadObject_Win32 win32实现
 @{
 */

/**
 @brief win32线程对象

  这个类的实现是平台相关的
 */
class TERARK_DLL_EXPORT Thread : public ThreadBase
{
	DECLARE_NONE_COPYABLE_CLASS(Thread)
	HANDLE m_handle;

public:
	Thread()
	{
		m_handle = 0;
	}
	~Thread()
	{
		if (0 != m_handle)
			CloseHandle(m_handle);
	}

public:
	static void sleep(int milliseconds)
	{
		::Sleep(milliseconds);
	}
	static void yield()
	{
		::Sleep(0);
	}

protected:
	static DWORD WINAPI threadProc(LPVOID param)
	{
		ThreadBase::threadProcImpl(param);
		return 0;
	}
	void do_start(int stackSize)
	{
		DWORD threadID;
		m_handle = CreateThread(0, stackSize, threadProc, this, 0, &threadID);
		if (0 == m_handle) {
			throw ThreadException("terark::thread::Thread::start(), when create thread");
		}
	}

	void do_join()
	{
		DWORD dwRet = WaitForSingleObject(m_handle, INFINITE);
		if (WAIT_OBJECT_0 != dwRet)
		{
			throw ThreadException("terark::thread::Thread::join()");
		}
	}
};

//@}
//@}

} } // namespace terark::thread


/* vim: set tabstop=4 : */
// file: terark/thread/posix/thread_impl.hpp

#ifndef __terark_thread_h__
#	error "can not include this file by user code!!"
#endif

#include <pthread.h>
#include <unistd.h>
#include "../thread_base.hpp"

namespace terark { namespace thread {

/**
 @addtogroup ThreadObject
 @{
 @defgroup ThreadObject_Posix posix实现
 @{
 */

/**
 @brief posix线程对象

  这个类的实现是平台相关的
 */
class Thread : public ThreadBase
{
	DECLARE_NONE_COPYABLE_CLASS(Thread)
	pthread_t m_handle;

public:
	Thread()
	{
		m_handle = 0;
	}
	~Thread()
	{
	}

public:
	static void sleep(int milliseconds)
	{
		::usleep(1000 * milliseconds);
	}
	static void yield()
	{
		pthread_yield();
	}

protected:
	static void* threadProc(void* param)
	{
		// convert return value from int to void*, to confirm with pthread
		// void* saved in m_exitCode,
		// if need other return value, use class data member!!
		ThreadBase::threadProcImpl(param);
		return (void*)0;
	}
	void do_start(int stackSize)
	{
		pthread_attr_t attr;
		pthread_attr_init(&attr);
		if (stackSize)
			pthread_attr_setstacksize(&attr, stackSize);

		int iRet = pthread_create(&m_handle, &attr, threadProc, (void *)this);
		pthread_attr_destroy(&attr);

		if (0 != iRet) {
			throw ThreadException("terark::thread::Thread::start(), when create thread");
		}
	}

	void do_join()
	{
		void* retVal;
		if (pthread_join(m_handle, &retVal) != 0)
		{
			throw ThreadException("terark::thread::Thread::join()");
		}
	}
};
//@}
//@}

} } // namespace terark::thread


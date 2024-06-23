/* vim: set tabstop=4 : */
// file: terark/thread/win32/SynchObject.hpp

#ifndef __terark_SynchObject_h__
#	error "can not include this file by user code!!"
#endif

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <WinBase.h>
#include <assert.h>

namespace terark { namespace thread {

/**
 @addtogroup SynchObject
 @{
 @defgroup SynchObject_Win32 win32 µœ÷
 @{
 */

/**
 @brief win32ª•≥‚À¯
 */
class TERARK_DLL_EXPORT MutexLock
{
	DECLARE_NONE_COPYABLE_CLASS(MutexLock)

	CRITICAL_SECTION m_cs;

public:
	typedef MutexLockSentryable<MutexLock> Sentry, MutexLockSentry;

	MutexLock()
	{
		InitializeCriticalSection(&m_cs);
	}
	~MutexLock()
	{
		DeleteCriticalSection(&m_cs);
	}

	void lock()
	{
		EnterCriticalSection(&m_cs);
	}
	void unlock()
	{
		LeaveCriticalSection(&m_cs);
	}
	bool trylock()
	{
		return !!TryEnterCriticalSection(&m_cs);
	}
};

/**
 @brief win32∂¡–¥À¯
 */
class TERARK_DLL_EXPORT ReadWriteLock
{
	DECLARE_NONE_COPYABLE_CLASS(ReadWriteLock)

public:
	typedef  ReadLockSentryable<ReadWriteLock>  ReadLockSentry;
	typedef WriteLockSentryable<ReadWriteLock> WriteLockSentry;

	ReadWriteLock();
	~ReadWriteLock();

	void readLock();
	void writeLock();
	void unlock();
	void tryReadLock();

	void tryWriteLock();

	// timeout is in milliseconds.
	bool readLock(int timeout);
	bool writeLock(int timeout);

private:
	CRITICAL_SECTION m_cs;    // Permits exclusive access to other members
	HANDLE m_hsemReaders;     // Readers wait on this if a writer has access
	HANDLE m_hsemWriters;     // Writers wait on this if a reader has access
	int    m_nWaitingReaders; // Number of readers waiting for access
	int    m_nWaitingWriters; // Number of writers waiting for access
	int    m_nActive;         // Number of threads currently with access
	//   (0=no threads, >0=# of readers, -1=1 writer)
};

/**
 @brief win32–≈∫≈¡ø
 */
class TERARK_DLL_EXPORT Semaphore
{
	DECLARE_NONE_COPYABLE_CLASS(Semaphore)

	HANDLE hSem;
	volatile int m_count;

public:
	explicit Semaphore(int count, int shared = 0);
	~Semaphore();

	void wait();
	bool wait(int timeout);

#if 0
	int wait(const ::timespec& abstime);
#endif

	void post();
	void post(int count);
	int getValue() throw();
};

//@}
//@}

} } // namespace terark::thread


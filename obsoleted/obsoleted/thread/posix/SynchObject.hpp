/* vim: set tabstop=4 : */
// file: terark/thread/posix/SynchObject_Posix.h

#ifndef __terark_SynchObject_h__
#	error "can not include this file by user code!!"
#endif

#include <pthread.h>
#include <semaphore.h>
#include <errno.h>
#include <sys/time.h>

namespace terark { namespace thread {

::timespec getExpireTime(int timeout); // timeout is in milliseconds

/**
 @addtogroup SynchObject
 @{
 @defgroup SynchObject_Posix posix µœ÷
 @{
 */

/**
 @brief posixª•≥‚À¯
 */
class MutexLock
{
	DECLARE_NONE_COPYABLE_CLASS(MutexLock)
	::pthread_mutex_t mutex;

public:
	typedef MutexLockSentryable<MutexLock> Sentry, MutexLockSentry;

	MutexLock();
	~MutexLock();

	void lock();
	void unlock();
	bool trylock();
};

/**
 @brief posix∂¡–¥À¯
 */
class ReadWriteLock
{
	DECLARE_NONE_COPYABLE_CLASS(ReadWriteLock)
	::pthread_rwlock_t rwlock;

public:
	typedef  ReadLockSentryable<ReadWriteLock>  ReadLockSentry;
	typedef WriteLockSentryable<ReadWriteLock> WriteLockSentry;

	ReadWriteLock();
	~ReadWriteLock();

	void readLock();
	void writeLock();
	void unlock();

	bool tryReadLock();
	bool tryWriteLock();

	// timeout is in milliseconds.
	bool readLock(int timeout);

	// timeout is in milliseconds.
	bool writeLock(int timeout);
};

/**
 @brief posix–≈∫≈¡ø
 */
class Semaphore
{
	DECLARE_NONE_COPYABLE_CLASS(Semaphore)
	sem_t sem;

public:
	explicit Semaphore(int count, int shared = 0);
	~Semaphore();

	void wait();
	bool wait(int timeout);
	bool wait(const ::timespec& abstime);

	void post();

#if 0 // current not supported
	void post(int count);
#endif

	int getValue();
};

//@}
//@}

} } // namespace terark::thread


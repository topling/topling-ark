/* vim: set tabstop=4 : */
#include "../SynchObject.hpp"
#include <string>

namespace terark { namespace thread {

void throw_synch_api_error_exception(int error, const char* method, const char* api)
{
	assert(0 != error);

	std::string text;
	text += method;
	text += " , on api: ";
	text += api;

	if (EAGAIN == error) {
		text += ", error=EAGAIN\nThe system lacked the necessary resources (other than memory) to initialize the object";
		throw ThreadException(text.c_str());
	}
	else if (ENOMEM == error) {
		text += ", error=ENOMEM\nInsufficient memory exists to initialize the object";
		throw ThreadException(text.c_str());
	}
	else if (EDEADLK == error) {
		text += ", error=EDEADLK\nA deadlock condition was detected";
		throw DeadLockException(text.c_str());
	}
	else if (EINTR == error) {
		text += ", error=EINTR\nA signal interrupted the api";
		throw LockException(text.c_str());
	}
	else if (EPERM == error) {
		text += ", error=EPERM\nThe current thread does not hold a lock";
		throw LockException(text.c_str());
	}
	else if (EINVAL == error) {
		text += ", error=EINVAL\ninvalid argument";
		throw std::invalid_argument(text);
	}
	else {
		char szBuf[64];
		sprintf(szBuf, ", error=%d(0x%04X)\nunknow exception", error, error);
		text += szBuf;
		throw LockException(text.c_str());
	}
}

::timespec getExpireTime(int timeout) // timeout is in milliseconds
{
	::timespec ts;
	::timeval  tv;
	::gettimeofday(&tv, NULL);
	ts.tv_nsec = 1000 * (tv.tv_usec + (timeout % 1000) * 1000);
	ts.tv_sec  = tv.tv_sec + timeout / 1000 + ts.tv_nsec / (1000 * 1000 * 1000);
	ts.tv_nsec %= 1000 * 1000 * 1000;

	return ts;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

MutexLock::MutexLock()
{
	//	mutex =  PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
	if (0 == ::pthread_mutex_init(&mutex, 0)) return;
	throw_synch_api_error_exception(
			errno, "terark::thread::MutexLock::MutexLock()", "pthread_mutex_init");
}
MutexLock::~MutexLock()
{
	int error = ::pthread_mutex_destroy(&mutex);
	if (0 == error) return;
	if (EBUSY == error) {
		throw LockException("terark::thread::MutexLock::~MutexLock(), the mutex is currently locked");
	}
	throw_synch_api_error_exception(
			error, "terark::thread::MutexLock::~MutexLock()", "pthread_mutex_destroy");
}

void MutexLock::lock()
{
	int error = ::pthread_mutex_lock(&mutex);
	if (0 != error) {
		throw_synch_api_error_exception(
				error, "terark::thread::MutexLock::lock()", "pthread_mutex_lock");
	}
}
void MutexLock::unlock()
{
	int error = ::pthread_mutex_unlock(&mutex);
	if (0 != error) {
		throw_synch_api_error_exception(
				error, "terark::thread::MutexLock::unlock()", "pthread_mutex_unlock");
	}
}
bool MutexLock::trylock()
{
	int error = ::pthread_mutex_trylock(&mutex);
	if (0 == error) return true;
	if (EBUSY == error)	return false;
	throw_synch_api_error_exception(
			error, "terark::thread::MutexLock::trylock()", "pthread_mutex_trylock");
}

//////////////////////////////////////////////////////////////////////////////////////////////////
ReadWriteLock::ReadWriteLock()
{
	int error = ::pthread_rwlock_init(&rwlock, 0);
	if (0 == error) return;
	if (EPERM == error) {
		throw LockException("terark::thread::ReadWriteLock::ReadWriteLock(), "
				"on api: pthread_rwlock_init, "
				"The caller does not have sufficient privilege to per-perform the operation"
				);
	} else {
		throw_synch_api_error_exception(error,
				"terark::thread::ReadWriteLock::ReadWriteLock()", "pthread_rwlock_init");
	}
}
ReadWriteLock::~ReadWriteLock()
{
	int error = ::pthread_rwlock_destroy(&rwlock);
	if (0 != error) {
		throw_synch_api_error_exception(error,
				"terark::thread::ReadWriteLock::~ReadWriteLock()", "pthread_rwlock_destroy");
	}
}

void ReadWriteLock::readLock()
{
	int error = ::pthread_rwlock_rdlock(&rwlock);
	if (0 != error) {
		throw_synch_api_error_exception(error,
				"terark::thread::ReadWriteLock::readLock()", "pthread_rwlock_rdlock");
	}
}

void ReadWriteLock::writeLock()
{
	int error = ::pthread_rwlock_wrlock(&rwlock);
	if (0 != error) {
		throw_synch_api_error_exception(error,
				"terark::thread::ReadWriteLock::writeLock()", "pthread_rwlock_wrlock");
	}
}

void ReadWriteLock::unlock()
{
	int error = ::pthread_rwlock_unlock(&rwlock);
	if (0 != error) {
		throw_synch_api_error_exception(error,
				"terark::thread::ReadWriteLock::unlock()", "pthread_rwlock_unlock");
	}
}

bool ReadWriteLock::tryReadLock()
{
	int error = pthread_rwlock_tryrdlock(&rwlock);
	if (0 == error) return true;
	if (EBUSY == error) return false;
	throw_synch_api_error_exception(error,
			"terark::thread::ReadWriteLock::tryReadLock()", "pthread_rwlock_tryrdlock");
}

bool ReadWriteLock::tryWriteLock()
{
	int error = pthread_rwlock_trywrlock(&rwlock);
	if (0 == error) return true;
	if (EBUSY == error) return false;
	throw_synch_api_error_exception(error,
			"terark::thread::ReadWriteLock::tryWriteLock()", "pthread_rwlock_trywrlock");
}

// timeout is in milliseconds.
bool ReadWriteLock::readLock(int timeout)
{
#if 0
	::timespec abstime = getExpireTime(timeout);
	int error = ::pthread_rwlock_timedrdlock(&rwlock, &abstime);
	if (0 == error) return true;
	if (ETIMEDOUT == error) return false;
	throw_synch_api_error_exception(error,
			"terark::thread::ReadWriteLock::readLock(int timeout)", "pthread_rwlock_timedrdlock");
#else
	assert(!"Unsupported\n");
	return false;
#endif
}

// timeout is in milliseconds.
bool ReadWriteLock::writeLock(int timeout)
{
#if 0
	::timespec abstime = getExpireTime(timeout);
	int error = pthread_rwlock_timedwrlock(&rwlock, &abstime);
	if (0 == error) return true;
	if (ETIMEDOUT == error) return false;
	throw_synch_api_error_exception(error,
			"terark::thread::ReadWriteLock::writeLock(int timeout)", "pthread_rwlock_timedwrlock");
#else
	assert(!"Unsupported\n");
	return false;
#endif
}

//////////////////////////////////////////////////////////////////////////////////////////////////

Semaphore::Semaphore(int count, int shared)
{
	int error = ::sem_init(&sem, shared, count);
	assert(0 == error);
}

Semaphore::~Semaphore()
{
   	sem_destroy(&sem);
}

void Semaphore::wait()
{
	do {
		if (0 == ::sem_wait(&sem)) return;
	} while (EINTR == errno);

	throw_synch_api_error_exception(
			errno, "terark::thread::Semaphore::wait()", "sem_wait");
}

bool Semaphore::wait(int timeout)
{
	::timespec abstime = getExpireTime(timeout);
	return wait(abstime);
}

bool Semaphore::wait(const ::timespec& abstime)
{
	if (0 == ::sem_timedwait(&sem, &abstime)) return true;
	int error = errno;
	if (ETIMEDOUT == error)	return false;
	if (EINTR == error)	return false;
	throw_synch_api_error_exception(
			error, "terark::thread::Semaphore::wait(int timeout)", "sem_timedwait");
}

void Semaphore::post()
{
	if (0 == ::sem_post(&sem)) return;
	throw_synch_api_error_exception(
			errno, "terark::thread::Semaphore::post()", "sem_post");
}

#if 0 // current not supported
void Semaphore::post(int count)
{
	if (0 == ::sem_post_multiple(&sem, count)) return;
	throw_synch_api_error_exception(
			errno, "terark::thread::Semaphore::post(int count)", "sem_post_multiple");
}
#endif

int Semaphore::getValue()
{
	int value;
	if (0 == ::sem_getvalue(&sem, &value))
		return value;
	else {
		throw_synch_api_error_exception(
				errno, "terark::thread::Semaphore::getValue()", "sem_getvalue");
	}
}

} } // namespace terark::thread


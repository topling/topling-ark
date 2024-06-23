/* vim: set tabstop=4 : */
// file: terark/thread/win32/SynchObject.cpp

#include "../SynchObject.hpp"

namespace terark { namespace thread {

ReadWriteLock::ReadWriteLock()
{
	// Initially no readers want access, no writers want access, and
	// no threads are accessing the resource
	m_nWaitingReaders = m_nWaitingWriters = m_nActive = 0;
	m_hsemReaders = CreateSemaphore(NULL, 0, MAXLONG, NULL);
	m_hsemWriters = CreateSemaphore(NULL, 0, MAXLONG, NULL);
	InitializeCriticalSection(&m_cs);
}

ReadWriteLock::~ReadWriteLock()
{
	// A SWMRG shouldn't be destroyed if any threads are using the resource
	assert (0 == m_nActive);

	m_nWaitingReaders = m_nWaitingWriters = m_nActive = 0;
	DeleteCriticalSection(&m_cs);
	CloseHandle(m_hsemReaders);
	CloseHandle(m_hsemWriters);
}

void ReadWriteLock::readLock()
{
	// Ensure exclusive access to the member variables
	EnterCriticalSection(&m_cs);

	// Are there writers waiting or is a writer writing?
	BOOL fResourceWritePending = (m_nWaitingWriters || (m_nActive < 0));

	if (fResourceWritePending) {

		// This reader must wait, increment the count of waiting readers
		m_nWaitingReaders++;
	} else {

		// This reader can read, increment the count of active readers
		m_nActive++;
	}

	// Allow other threads to attempt reading/writing
	LeaveCriticalSection(&m_cs);

	if (fResourceWritePending) {

		// This thread must wait
		WaitForSingleObject(m_hsemReaders, INFINITE);
	}
}

void ReadWriteLock::writeLock()
{
	// Ensure exclusive access to the member variables
	EnterCriticalSection(&m_cs);

	// Are there any threads accessing the resource?
	BOOL fResourceOwned = (m_nActive != 0);

	if (fResourceOwned) {

		// This writer must wait, increment the count of waiting writers
		m_nWaitingWriters++;
	} else {

		// This writer can write, decrement the count of active writers
		m_nActive = -1;
	}

	// Allow other threads to attempt reading/writing
	LeaveCriticalSection(&m_cs);

	if (fResourceOwned) {

		// This thread must wait
		WaitForSingleObject(m_hsemWriters, INFINITE);
	}
}

void ReadWriteLock::unlock()
{
	// Ensure exclusive access to the member variables
	EnterCriticalSection(&m_cs);

	if (m_nActive > 0) {

		// Readers have control so a reader must be done
		m_nActive--;
	} else {

		// Writers have control so a writer must be done
		m_nActive++;
	}

	HANDLE hsem = NULL;  // Assume no threads are waiting
	LONG lCount = 1;     // Assume only 1 waiter wakes; always true for writers

	if (m_nActive == 0) {

		// No thread has access, who should wake up?
		// NOTE: It is possible that readers could never get access
		//       if there are always writers wanting to write

		if (m_nWaitingWriters > 0) {

			// Writers are waiting and they take priority over readers
			m_nActive = -1;         // A writer will get access
			m_nWaitingWriters--;    // One less writer will be waiting
			hsem = m_hsemWriters;   // Writers wait on this semaphore
			// NOTE: The semaphore will release only 1 writer thread

		} else if (m_nWaitingReaders > 0) {

			// Readers are waiting and no writers are waiting
			m_nActive = m_nWaitingReaders;   // All readers will get access
			m_nWaitingReaders = 0;           // No readers will be waiting
			hsem = m_hsemReaders;            // Readers wait on this semaphore
			lCount = m_nActive;              // Semaphore releases all readers
		} else {

			// There are no threads waiting at all; no semaphore gets released
		}
	}

	// Allow other threads to attempt reading/writing
	LeaveCriticalSection(&m_cs);

	if (hsem != NULL) {

		// Some threads are to be released
		ReleaseSemaphore(hsem, lCount, NULL);
	}
}

void ReadWriteLock::tryReadLock()
{
	assert(!"Unsupported");
}
void ReadWriteLock::tryWriteLock()
{
	assert(!"Unsupported");
}

// timeout is in milliseconds.
bool ReadWriteLock::readLock(int timeout)
{
	assert(!"Unsupported");
	return false;
}

// timeout is in milliseconds.
bool ReadWriteLock::writeLock(int timeout)
{
	assert(!"Unsupported");
	return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

Semaphore::Semaphore(int count, int shared)
{
   	hSem = CreateSemaphore(0, count, INT_MAX, 0);
	m_count = count;
}

Semaphore::~Semaphore()
{
	if (0 != hSem) CloseHandle(hSem);
}

void Semaphore::wait()
{
	DWORD dwRet = WaitForSingleObject(hSem, INFINITE);
	if (WAIT_OBJECT_0 == dwRet)
		--m_count;
	else {
		throw LockException("terark::thread::Semaphore::wait(), WaitForSingleObject");
	}
}

bool Semaphore::wait(int timeout)
{
	DWORD dwRet = WaitForSingleObject(hSem, (DWORD)timeout);
	if (WAIT_OBJECT_0 == dwRet) {
		--m_count;
		return true;
	} else if (WAIT_TIMEOUT == dwRet)
		return false;
	else if (WAIT_FAILED == dwRet) {
		throw LockException("terark::thread::Semaphore::wait(), WaitForSingleObject() = WAIT_FAILED");
	} else
		throw LockException("terark::thread::Semaphore::wait(), WaitForSingleObject() = unknow");
}

#if 0
int Semaphore::wait(const ::timespec& abstime) throw()
{
	assert(!"Unsupported");
}
#endif

void Semaphore::post()
{
	LONG prevCount;
	if (ReleaseSemaphore(hSem, 1, &prevCount))
		++m_count;
	else {
		throw LockException("terark::thread::Semaphore::post(), ReleaseSemaphore");
	}
}

void Semaphore::post(int count)
{
	LONG prevCount;
	if (ReleaseSemaphore(hSem, count, &prevCount))
		m_count += count;
	else {
		throw LockException("terark::thread::Semaphore::post(int count), ReleaseSemaphore");
	}
}

int Semaphore::getValue() throw()
{
	return m_count;
}

} } // namespace terark::thread


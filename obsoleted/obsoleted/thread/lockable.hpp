/* vim: set tabstop=4 : */
#ifndef __terark_thread_lockable_h__
#define __terark_thread_lockable_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#include <assert.h>
#include <terark/stdtypes.hpp>
#include "../thread/SynchObject.hpp"

namespace terark { namespace thread {

/**
 @addtogroup thread
 @{
 @defgroup lockable 为现有类增加锁功能
 @{

  对于任何一个类 Class，使用 SomeLockable<Class> 就可以使这个类可以被 SomeLock 。
  这样，就使得锁和对象合为一体，用户程序就不用担心哪个对象用的是哪个锁

  @code
	Class1 x, y;
	MutexLock lock1, lock2;
	{ // code1
	   MutexLock::MutexLockSentry sentry(lock1);
	   x.func();
	   // do something...
	}
	//...
	{ // code2
	   MutexLock::MutexLockSentry sentry(lock1); // should be lock2
	   // do something...
	   y.func();
	}
	//...
  @endcode
	使用 MutexLockable....
  @code
	typedef MutexLockable<Class1> MutexClass1;
	MutexClass1 x, y;
	{ // code1
	   MutexClass1::MutexLockSentry sentry(x);
	   x.func();
	   //...
	}
	//....
	{ // code2
	   MutexClass1::MutexLockSentry sentry(y);
	   y.func();
	   //...
	}
  @endcode
 */

/**
 @defgroup LockableTraits 可锁定的种类

  通过 LockableTraits<Class>::LockableTag 来标明 Class 的锁类型
  @{
 */

/// 锁定 tag 的基类，任意锁定类型
struct AnyLockableTag {};

/// 不可锁定
struct NoneLockableTag  : public AnyLockableTag {};

/// 可互斥锁
struct MutexLockableTag : public AnyLockableTag {};

/// 可信号量锁
struct SemaphoreableTag : public AnyLockableTag {};

/// concurrent queue
struct PutGetLockableTag : public MutexLockableTag {};

/// 可读写锁
struct ReadWriteLockableTag : public AnyLockableTag {};

/// 可锁定容器元素
struct ContainerElementLockableTag : public ReadWriteLockableTag {};

/// 为定义 Traits 的类型是不可锁的
template<class Class>
struct LockableTraits
{
	typedef NoneLockableTag LockableTag;
};

//@}

/**
 @brief 该类模板或许有用
 */
template<class Class>
class DummyLockable : public Class
{
public:
	typedef Class Base;

	void lock()    const throw() { }
	void unlock()  const throw() { }
	void trylock() const throw() { }
};
template<class Class>
struct LockableTraits<DummyLockable<Class> >
{
	typedef NoneLockableTag LockableTag; ///< 不可锁
};
class DummyLock : public DummyLockable<EmptyClass> {};

template<class Class>
class MutexLockable : public Class
{
	MutexLock m_mutex;

public:
	typedef MutexLockSentryable<MutexLockable> MutexLockSentry;

	void lock()    { m_mutex.lock(); }
	void unlock()  { m_mutex.unlock(); }
	bool trylock() { return m_mutex.trylock(); }
};

template<class Class>
struct LockableTraits<MutexLockable<Class> >
{
	typedef MutexLockableTag LockableTag; ///< 互斥锁
};

/**
 @brief 可互斥锁对象的代理
 */
template<class MutexLockableClass>
class MutexLockableProxy
{
	MutexLockableClass* m_pLockableObject;

public:
	typedef MutexLockSentryable<MutexLockableProxy> MutexLockSentry;

	MutexLockableProxy(MutexLockableClass* destObject)
		: m_pLockableObject(destObject) { }

	void lock()    { m_pLockableObject->lock(); }
	void unlock()  { m_pLockableObject->unlock(); }
	bool trylock() { return m_pLockableObject->trylock(); }
};
template<class Class>
struct LockableTraits<MutexLockableProxy<Class> >
{
	typedef MutexLockableTag LockableTag;
};

/**
 @brief 可互斥锁对象的扩展代理
 */
template<class Class, class MutexLockableClass>
class ExtMutexLockable :
	public Class, public MutexLockableProxy<MutexLockableClass>
{
public:
	ExtMutexLockable(MutexLockableClass* pMutexLockable)
		: MutexLockableProxy<MutexLockableClass>(pMutexLockable)
	{ }
	typedef MutexLockSentryable<ExtMutexLockable> MutexLockSentry;
};

template<class Class, class MutexLockableClass>
struct LockableTraits<ExtMutexLockable<Class, MutexLockableClass> >
{
	typedef MutexLockableTag LockableTag;
};

/**
 @brief 可读写锁
 */
template<class Class>
class ReadWriteLockable : public Class
{
	ReadWriteLock m_rwlock;

public:
	typedef  ReadLockSentryable<ReadWriteLockable>  ReadLockSentry;
	typedef WriteLockSentryable<ReadWriteLockable> WriteLockSentry;

	void readLock()  { m_rwlock.readLock();  }
	void writeLock() { m_rwlock.writeLock(); }
	void unlock()	 { m_rwlock.unlock();    }

	///@brief timeout is in milliseconds.
	bool readLock(int timeout)  { return m_rwlock.readLock(timeout);  }
	bool writeLock(int timeout) { return m_rwlock.writeLock(timeout); }
};
template<class Class>
struct LockableTraits<ReadWriteLockable<Class> >
{
	typedef ReadWriteLockableTag LockableTag;
};

/**
 @brief 可读写锁对象的代理
 */
template<class ReadWriteLockableClass>
class ReadWriteLockableProxy
{
	ReadWriteLockableClass* m_pLockableObject;

public:
	typedef  ReadLockSentryable<ReadWriteLockableProxy>  ReadLockSentry;
	typedef WriteLockSentryable<ReadWriteLockableProxy> WriteLockSentry;

	ReadWriteLockableProxy(ReadWriteLockableClass* destObject)
		: m_pLockableObject(destObject) { }

	void readLock()  { return m_pLockableObject->readLock(); }
	void writeLock() { return m_pLockableObject->writeLock(); }

	void unlock()	 { return m_pLockableObject->unlock(); }

	bool readLock(int timeout)  { return m_pLockableObject->readLock(timeout); }
	bool writeLock(int timeout) { return m_pLockableObject->writeLock(timeout); }
};
template<class Class>
struct LockableTraits<ReadWriteLockableProxy<Class> >
{
	typedef ReadWriteLockableTag LockableTag;
};

/**
 @brief 可读写锁对象的扩展代理
 */
template<class Class, class ReadWriteLockableClass>
class ExtReadWriteLockable :
	public Class, public ReadWriteLockableProxy<ReadWriteLockableClass>
{
public:
	ExtReadWriteLockable(ReadWriteLockableClass* pMutexLockable)
		: ReadWriteLockableProxy<ReadWriteLockableClass>(pMutexLockable)
	{ }
	typedef  ReadLockSentryable<ExtReadWriteLockable>  ReadLockSentry;
	typedef WriteLockSentryable<ExtReadWriteLockable> WriteLockSentry;
};

template<class Class, class ReadWriteLockableClass>
struct LockableTraits<ExtReadWriteLockable<Class, ReadWriteLockableClass> >
{
	typedef ReadWriteLockableTag LockableTag;
};

/**
 @brief 可信号对象
 */
template<class Class>
class Semaphoreable : public Class
{
protected:
	Semaphore m_sem;
private:
	Semaphoreable(const Semaphoreable&);
	Semaphoreable& operator=(const Semaphoreable&);
public:
	Semaphoreable(int count, int shared = 0)
		: m_sem(count, shared)
   	{ }

	void wait() { m_sem.wait(); }

	bool wait(int timeout) { return m_sem.wait(timeout); }
	void post() { m_sem.post(); }

#if defined(_WIN32) || defined(WIN32)
	void post(int count) { m_sem.post(count); }
#else
	bool wait(const ::timespec& abstimeout) { return m_sem.wait(abstimeout); }
#endif

	int getValue() { return m_sem.getValue();	}
};
template<class Class>
struct LockableTraits<Semaphoreable<Class> >
{
	typedef SemaphoreableTag LockableTag;
};

//@} // end defgroup
//@} // end addtogroup

} } // namespace terark::thread

#endif // __terark_thread_lockable_h__


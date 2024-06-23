/* vim: set tabstop=4 : */
#ifndef PooledLock_h__
#define PooledLock_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#include "../HeavyObjectPool.hpp"

namespace terark { namespace thread {

//@{
/**
 @ingroup thread
 @brief 缓冲的锁

 当一个应用需要非常频繁地创建/销毁锁的时候，创建和销毁锁的开销占的系统总开销比重就很大了\n
 提供这个类的目的就是减少创建/销毁锁的开销
 */
class PooledMutex : public PooledObject<MutexLock>
{
	DECLARE_NONE_COPYABLE_CLASS(PooledMutex)
public:
	PooledMutex() {}
	void lock()	   { m_proxy->lock();    }
	void unlock()  { m_proxy->unlock();  }
	bool trylock() { return m_proxy->trylock(); }
};

class PooledReadWriteLock : public PooledObject<ReadWriteLock>
{
	DECLARE_NONE_COPYABLE_CLASS(PooledReadWriteLock)
public:
	PooledReadWriteLock() {}

	void readLock()  { m_proxy->readLock();  }
	void writeLock() { m_proxy->writeLock(); }
	void unlock()	 { m_proxy->unlock();    }

	bool readLock(int timeout)  { return m_proxy->readLock(timeout); }
	bool writeLock(int timeout) { return m_proxy->writeLock(timeout); }
};
//@}

} } // namespace terark::thread

#endif // PooledLock_h__

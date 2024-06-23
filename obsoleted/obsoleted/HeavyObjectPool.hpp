/* vim: set tabstop=4 : */
#ifndef HeavyObjectPool_h__
#define HeavyObjectPool_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
# pragma warning(push)
# pragma warning(disable: 4018) // '<' : signed/unsigned mismatch
# pragma warning(disable: 4267) // 'return' : conversion from 'size_t' to 'int', possible loss of data
#endif

#include <terark/util/concurrent_queue.hpp>
#include "ObjectHolder.hpp"
#include <list>
#include <queue>

namespace terark {

/**
 @brief 重量级对象缓冲池

  适用于互相对等的类，如各种“锁对象”
  可以减少对象的创建开销
 */
template<class HeavyObject>
class HeavyObjectPool
{
	DECLARE_NONE_COPYABLE_CLASS(HeavyObjectPool)

	typedef ObjectHolder<HeavyObject>							ElemHolder;
	typedef thread::concurrent_queue<std::queue<HeavyObject*> >	FreeListType;
	typedef std::list<ElemHolder>								HolderContainer;

private:
	FreeListType    m_freeList;
	HolderContainer m_holder;
	int m_maxFreeCount;

public:
	typedef HeavyObject  value_type;
	typedef thread::MutexLockSentryable<HeavyObjectPool> MutexLockSentry;

	/**
	 it would actually create maxSize objects in the pool
	 it would create new object when m_freeList is empty,
	 and the max size of the pool is maxSize.
	 if the pool size reach maxSize,
	 it will wait until a thread give back an object
	 */
	HeavyObjectPool(int maxSize = INT_MAX, int maxFreeCount = 8)
		: m_freeList(maxSize)
	{
		assert(maxFreeCount <= maxSize);
		m_maxFreeCount = maxFreeCount;
	}

	~HeavyObjectPool()
	{
		typename HolderContainer::iterator iter = m_holder.begin();
		for (; iter != m_holder.end(); ++iter)
		{
			(*iter).destroy();
		}
	}

	//! take an object, maybe from free list, maybe created new
	HeavyObject* take()
	{
		{
			MutexLockSentry sentry(*this);
			if (m_freeList.peekEmpty())
			{
				if (m_holder.size() < m_freeList.maxSize())
				{
					m_holder.push_back(ElemHolder());
					m_holder.back().construct();
					return &m_holder.back().value();
				}
			}
		}
		return m_freeList.pop();
	}

	//! give back an object to the pool
	void giveBack(HeavyObject* x)
	{
		bool shouldGiveBack = true;
		{
			MutexLockSentry sentry(*this);
			if (m_freeList.peekSize() >= m_maxFreeCount)
			{
				// free object count is exceeded.
				// sequential find the object and destroy it!
				typename HolderContainer::iterator iter = m_holder.begin();
				for (;iter != m_holder.end(); ++iter)
				{
					if (&(*iter).value() == x)
					{ // must run to here...
						(*iter).destroy();
						m_holder.erase(iter);
						break;
					}
				}
				assert(m_holder.end() != iter); // must found x

				// already destroyed, should not give back to free list..
				shouldGiveBack = false;
			}
		}
		if (shouldGiveBack)
			m_freeList.push(x); // put the object to free list
	}

public:
/**
 @name unusually used methods....
 @{
	manually put some object to the pool,
	these operations are not locked, must manually lock for operate!!
	for example:
 @code
	pool.lock();
	while (pool.size() < pool.maxSize())
	{
		pool.putNew(param1, pram2, pram3); // or...
	//	HeavyObject obj(.....);
	//	pool.putNew(obj);
	}
	pool.unlock();
 @endcode
*/
	int size() const throw() { return m_holder.size(); }
	int maxSize() const throw() { return m_freeList.maxSize(); }
	int freeCount() const throw() { return m_freeList.peekSize(); }
	int maxFreeCount() const throw() { return m_maxFreeCount; }

	void lock()	   { m_freeList.lock();    }
	void unlock()  { m_freeList.unlock();  }
	bool trylock() { return m_freeList.trylock(); }

	void putNew(const HeavyObject& x)
	{
		m_holder.push_back(ElemHolder());
		m_holder.back().construct(x);
	}

	template<class T1>
	void putNew(const T1& p1)
	{
		m_holder.push_back(ElemHolder());
		m_holder.back().construct(p1);
	}

	template<class T1, class T2>
	void putNew(const T1& p1, const T2& p2)
	{
		m_holder.push_back(ElemHolder());
		m_holder.back().construct(p1, p2);
	}

	template<class T1, class T2, class T3>
	void putNew(const T1& p1, const T2& p2, const T3& p3)
	{
		m_holder.push_back(ElemHolder());
		m_holder.back().construct(p1, p2, p3);
	}
//@}
};

namespace thread {

	template<class HeavyObject>
	struct LockableTraits<HeavyObjectPool<HeavyObject> >
	{
		typedef MutexLockableTag LockableTag;
	};

} // namespace thread

/**
 @brief 被缓冲的对象
 */
template<class HeavyObject> class PooledObject
{
	DECLARE_NONE_COPYABLE_CLASS(PooledObject)

protected:
	HeavyObject* m_proxy;
	typedef HeavyObjectPool<HeavyObject> PoolType;
	static  PoolType *cm_thePool;

public:
	PooledObject()   { assert(cm_thePool); m_proxy = cm_thePool->take();  }
	~PooledObject()  { assert(cm_thePool); cm_thePool->giveBack(m_proxy); }

	static void initPool(int maxSize = INT_MAX, int maxFreeCount = 10)
	{
		assert(0 == cm_thePool);
		cm_thePool = new PoolType(maxSize, maxFreeCount);
	}
	static void destroyPool()
	{
	#if defined(_DEBUG) || defined(DEBUG) || !defined(NDEBUG)
		assert(0 != cm_thePool);
		{
			typename PoolType::MutexLockSentry sentry(*cm_thePool);
			assert(cm_thePool->freeCount() == cm_thePool->size());
		}
	#endif
		delete cm_thePool;
		cm_thePool = 0;
	}
};
template<class HeavyObject>
HeavyObjectPool<HeavyObject>* PooledObject<HeavyObject>::cm_thePool = 0;

} // namespace terark

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma warning(pop)
#endif

#endif // HeavyObjectPool_h__

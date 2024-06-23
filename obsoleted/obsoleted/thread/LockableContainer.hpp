/* vim: set tabstop=4 : */
/********************************************************************
	@file LockableContainer.hpp
	@brief 提供一个容器，其中包含的元素可以被加锁，并且可以被安全地删除

	@date	2006-9-28 15:00
	@author	Lei Peng
	@{
*********************************************************************/
#ifndef LockableContainer_h__
#define LockableContainer_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#if defined(_WIN32) || defined(WIN32) || defined(_WIN64) || defined(WIN64)
# define WIN32_LEAN_AND_MEAN
# define NOMINMAX
# include <windows.h>
#endif

#include <boost/detail/atomic_count.hpp>

#include "PooledLock.hpp"
#include "thread.hpp"
//#include <map>
//#include <set>

//#include <terark/util/refcount.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/ordered_index.hpp>
//#include <boost/multi_index/member.hpp>
//#include <boost/multi_index/sequenced_index.hpp>
#include <boost/smart_ptr.hpp>

namespace terark { namespace thread {

/**
 @ingroup thread
 @defgroup LockableContainer LockableContainer
 @{

 @par 现实中往往有如下的需求
 -# LockableContainer 中的元素可能会被锁住较长时间
 -# LockableContainer 本身不应该被锁住太长时间
 -# LockableContainer 中的元素数量可能很大，而同一时刻只会有很小一部分元素需要加锁

 @par 因此，实现这样一个容器就需要考虑以上这些因素，并作出如下设计决策
 -# 为了提高并发，并且安全地为元素加锁，容器和元素的锁周期图示如下：
		- 容器的锁：  [lock------unlock]
		- 元素的引用：[live------------|---------------------------dead]
		- 元素的锁：      [lock--------|-----------------------unlock]
		-     或者：                   |     [lock-------------unlock]
 -# 如果一个线程正在等待 e 的锁，而另外的线程要删除元素 e，此时，\n
    删除 e 的线程应该以某种方式通知等待加锁的线程，e 要无效了，\n
	你还是不要等了。也就是说，是删除优先的！

 @par 因此，总是按如下步骤为元素加锁并使用：\n
//     1. 为 Container 加锁（读锁或写锁）\n
//     2. 从 Container 中获得 iterator ，该 iterator 指向 e\n
//     3. 增加 e 的引用计数\n
//     4. 为 Container 解锁\n
//	   5. 为 e 加锁\n
//     6. 使用 e\n
//     7. 为 e 解锁\n
//     8. 减少 e 的引用计数\n
//
// 使用这么复杂的步骤主要是为了增加第 5 步的并发性和安全性。否则要为 e 加锁就\n
// 必须锁住整个 Container ，或者一旦其它线程要删除 e ，当前线程却不能得到通知，\n
// 于是删除 e 的线程必须等待。\n
//\n
//     在实现中第 5 步的失败（其它线程正在删除 e）是由异常触发的，因此第 8 步\n
// 必须使用 sentry 技术（因为C++中没有 try ... finally ...）。所以，实际中的\n
// 代码应该如下：
@code
	// using container element:
	container.readLock(); // or writeLock();				// step 1
	LockableContainer::iterator iter = container.find(...); // step 2
	LockableContainer::ElemRefPtr pElemRef = container.getRefPtr(iter);// step 3, auto
	container.unlock();										// step 4
	pElemRef->lock(); // ElemRefCounterClass's member		// step 5
	// use iter...											// step 6
	// ...
	pElemRef->unlock(); // ElemRefCounterClass's member		// step 7
	// ..................................................	// step 8 auto by pElemRef's destructor
@endcode
 step5 和 step7 也可以通过 LockSentry 来解决，典型的代码如下：
@code
	LockableContainerType::WriteLockSentry containerSentry(container);
	LockableContainer::iterator iter = container.find(...);
	if (container.end() != iter)
	{
		LockableContainer::ElemRefPtr pElemRef = container.getRefPtr(iter);
		containerSentry.release();  // unlock container now
		MutexLockSentry elemSentry(pElemRef->value());

		// use iter or pElemRef...
		// ...
	}
@endcode
 从理论上讲，对于锁，可以只用 sentry 就可以解决，而不需要 sentry.release()，象下面这样使用是最好的：
@code
	LockableContainer::ElemRefPtr pElemRef;
	{ // a scope
		LockableContainerType::WriteLockSentry containerSentry(container);
		LockableContainer::iterator iter = container.find(...);
		if (container.end() != iter)
			container.getRefPtr(pElemRef, iter);
	}
	if (pElemRef)
	{
		MutexLockSentry elemSentry(pElemRef->value());

		// use pElemRef...
		// ...
	}
@endcode

// @par 如果要安全地删除元素，总按如下步骤进行
@code
	container.writeLock();
	LockableContainer::iterator iter = container.find(...);
	LockableContainer::ElemRefPtr pElemRef = container.getRefPtr(iter);
	container.unlock();
	container.erase(pElemRef);
@endcode
	or-----------
@code
	container.writeLock();
	LockableContainer::iterator iter = container.find(...);
	container.erase(iter);
	container.unlock();
@endcode

 @note 注意事项
  - Container 不能是 vector 等添加删除元素时会使所有 iterator 失效的容器！
  - ElemRefCounterClass 必须有兼容 ElemRefCounter 的接口！\n
		  一般使用 MutexElemLock 或者 ReadWriteElemLock

   @par 如果多个线程竞争容器中的同一元素 e ，其中有个线程要删除元素。

   @par 有以下两种情况：

	- 线程1 已经取得了对元素 e 的锁;\n
	    线程2 正在对元素 e 加锁;\n
		线程3 要删除元素 e;\n
		    在这种情况下，线程 3 会将该元素标记为“正在删除”，并等待，\n
		直到元素的引用计数回到 1，即只有线程3 拥有 e 的所有权。此时线程2\n
		会发现 e 的状态为“正在删除”，线程2 会抛出异常 ElementBeingErasedException，\n
		抛出异常将导致线程2 获取 e 的锁失败，并通过 ElemRefPtr\n
		的 destructor 解除了对 e 的所有权（将 e 的引用计数减一）。\n
		    然后线程3 等待线程1 归还 e 的锁，当线程1 归还了 e 的锁，并解除了对 e\n
		的所有权，线程3 就删除了 e 。\n

	- 线程1 已经取得了对元素 e 的所有权，但还未取得 e 的锁;\n
	    线程2 要删除元素 e;\n
		    在这种情况下，线程2 必须等待，直到线程1 主动减少 e 的引用计数，\n
		或者在获取 e 的锁时抛出异常。\n
*/
//@} // defgroup

/**
 @name LockableContainer 的基础支持类

  只用于 LockableContainer，不建议做其它用途
  @{
 */

/**
 @brief 元素正在被删除的异常类型

  至少有一个线程提出了删除该元素的请求，但又有一个线程要锁定该元素时，就抛出这个异常
 */
class ElementBeingErasedException : public ThreadException
{
public:
	ElementBeingErasedException(const char* message = "terark::thread::ElementBeingErasedException")
		: ThreadException(message) { }
};

/**
 @brief 仅被 LockableContainer 使用的引用计数器

  该引用计数器同时提供了安全删除元素所需的基本支持功能
 */
class ElemRefCounter
{
	DECLARE_NONE_COPYABLE_CLASS(ElemRefCounter)

	boost::detail::atomic_count m_refCount;
	volatile bool m_isErasing;
	volatile bool m_isEraseOnRelease;

public:
	ElemRefCounter() throw()
		: m_refCount(0)
	{
		m_isErasing = false;
		m_isEraseOnRelease = false;
	}
	void addRef()		throw() { ++m_refCount; }
	long releaseRef()   throw() { return --m_refCount; }
	long getRefCount()  const throw() { return m_refCount; }

	bool isErasing() const throw() { return m_isErasing; }
	void setErasing(bool bIsErasing) throw() { m_isErasing = bIsErasing; }

	bool isEraseOnRelease() const throw() { return m_isEraseOnRelease; }
	void setEraseOnRelease(bool isEraseOnRelease) throw() { m_isEraseOnRelease = isEraseOnRelease; }
};

/**
 @brief 互斥锁和引用计数代理

  通过这个代理减少代码冗余

 @param Clazz 参数化继承 Clazz，它可以是 ElemRefCounter，也可以是 PooledMutex
  具体是哪个取决于怎么更加便利
 */
template<class Clazz>
class MutexElemLockRefCounterProxy : public Clazz
{
	DECLARE_NONE_COPYABLE_CLASS(MutexElemLockRefCounterProxy)
public:
	ElemRefCounter* m_pRefCounter;
	PooledMutex*	m_pLock;
public:
	MutexElemLockRefCounterProxy() { m_pRefCounter = 0; m_pLock = 0; }

	void lock()
	{
		while (m_pLock->trylock())
		{
			Thread::sleep(50);
			if (m_pRefCounter->isErasing())
				throw ElementBeingErasedException();
		}
	}
	void unlock()
	{
		m_pLock->unlock();
	}
	typedef MutexLockSentryable<MutexElemLockRefCounterProxy> MutexLockSentry;
};

/**
 @brief 互斥元素锁

 使用的锁是 PooledMutex，以便最有效地使用有限的锁资源减少锁的创建和销毁开销
 */
class MutexElemLock : public MutexElemLockRefCounterProxy<ElemRefCounter>
{
	DECLARE_NONE_COPYABLE_CLASS(MutexElemLock)
	PooledMutex m_mutex;
public:
	MutexElemLock() { m_pLock = &m_mutex; m_pRefCounter = this; }
};

/**
 @brief 双互斥元素锁

 使用的锁是 PooledMutex，以便最有效地使用有限的锁资源减少锁的创建和销毁开销
 每个元素将有两个互斥锁，可以用于需要多个互斥锁的情况，
 比如对 socket ，可以对读操作和写操作分别加锁
 */
class DualMutexElemLock : public ElemRefCounter
{
	DECLARE_NONE_COPYABLE_CLASS(DualMutexElemLock)
public:
	MutexElemLockRefCounterProxy<PooledMutex> send, recv;
	typedef MutexElemLockRefCounterProxy<PooledMutex>::MutexLockSentry MutexLockSentry;
public:
	DualMutexElemLock()
	{
		send.m_pRefCounter = this;
		recv.m_pRefCounter = this;
		send.m_pLock = &send;
		recv.m_pLock = &recv;
	}
};

/**
 @brief 读写锁和引用计数代理

  通过这个代理减少代码冗余

 @param Clazz 参数化继承 Clazz，它可以是 ElemRefCounter，也可以是 PooledReadWriteLock
  具体是哪个取决于怎么更加便利
 */
template<class Clazz>
class ReadWriteElemLockRefCounterProxy : public Clazz
{
	DECLARE_NONE_COPYABLE_CLASS(ReadWriteElemLockRefCounterProxy)
public:
	ElemRefCounter*		 m_pRefCounter;
	PooledReadWriteLock* m_pLock;

public:
	ReadWriteElemLockRefCounterProxy() { m_pRefCounter = 0; m_pLock = 0; }

	void readLock()
	{
		const int timeout = 50;
		do {
			if (m_pRefCounter->isErasing())
				throw ElementBeingErasedException();
		} while (0 != m_pLock->readLock(timeout));
	}
	void writeLock()
	{
		const int timeout = 50;
		do {
			if (m_pRefCounter->isErasing())
				throw ElementBeingErasedException();
		} while (0 != m_pLock->writeLock(timeout));
	}
	void unlock()
	{
		m_pLock->unlock();
	}
	typedef  ReadLockSentryable<ReadWriteElemLockRefCounterProxy>  ReadLockSentry;
	typedef WriteLockSentryable<ReadWriteElemLockRefCounterProxy> WriteLockSentry;
};

/**
 @brief 读写元素锁

 使用的锁是 PooledReadWriteLock，以便最有效地使用有限的锁资源减少锁的创建和销毁开销
 */
class ReadWriteElemLock :
	public ReadWriteElemLockRefCounterProxy<ElemRefCounter>
{
	DECLARE_NONE_COPYABLE_CLASS(ReadWriteElemLock)
	PooledReadWriteLock m_rwLock;
public:
	ReadWriteElemLock() { m_pLock = &m_rwLock; m_pRefCounter = this; }
};

/**
 @brief LockableContainer 容器中元素的引用

  通过该引用可以安全地访问、删除LockableContainer中的元素
 @note
   instance of this class can only created by 'LockableContainer'
   这个类的对象只能由LockableContainer创建
 */
template<class LockableContainerType, class ElemRefCounterClass>
class LockableContainer_ElemRef	: public ObjectHolder<ElemRefCounterClass>
{
	typedef typename LockableContainerType::iterator	  iterator;
	typedef typename LockableContainerType::value_type	  value_type;

	iterator m_iter;
	LockableContainerType* m_container;

public:
	LockableContainer_ElemRef(LockableContainerType* container, iterator iter) throw()
	{
		m_iter = iter;
		m_container = container;
	}
	iterator iter() const throw()
   	{
		assert(this);
	   	return m_iter;
   	}
	value_type& elem() const throw()
   	{
		assert(this);
	   	return const_cast<value_type&>(*m_iter);
   	}
	const value_type* constElemPtr() const throw() // as key
   	{
		assert(this);
	   	return &*m_iter;
   	}
	LockableContainerType* container() const throw()
	{
		assert(this);
		return m_container;
	}
};

//@} //name LockableContainer 的基础支持类

/**
 @ingroup LockableContainer
 @brief LockableContainer

 @param ElemRefCounterClass, default is MutexElemLock
	must be a derived class of ElemRefCounter
 */
template<class Container, class ElemRefCounterClass = MutexElemLock>
class LockableContainer : public ReadWriteLockable<Container>
{
	DECLARE_NONE_COPYABLE_CLASS(LockableContainer)

public:
	typedef typename Container::iterator   iterator;
	typedef typename Container::value_type value_type;
	typedef typename Container::reference  reference;
	typedef ElemRefCounterClass elem_ref_type;

protected:
// instance of MyElemRef can only created by 'LockableContainer'
	typedef LockableContainer_ElemRef<LockableContainer, ElemRefCounterClass> MyElemRef;
public:
//	typedef boost::intrusive_ptr<MyElemRef> ElemRefPtr;
	class ElemRefPtr : public boost::intrusive_ptr<MyElemRef>
	{
		typedef boost::intrusive_ptr<MyElemRef> BasePtr;
	public:
		ElemRefPtr& operator=(const ElemRefPtr& p)
		{
			// *this 和 p 至少要有一个为 NULL
			assert(0 == this->get() || 0 == p.get());

			this->BasePtr::operator=(p);
			return *this;
		}
		ElemRefPtr& operator=(MyElemRef* p)
		{
			// 如果没有这个函数，赋值时会调用 operator=(const ElemRefPtr& p)
			// 会导致多创建一个临时对象，并在销毁临时对象时死锁!!!
			// *this 和 p 至少要有一个为 NULL
			assert(0 == this->get() || 0 == p);

			this->BasePtr::operator=(p);
			return *this;
		}
		ElemRefPtr() {}
		ElemRefPtr(MyElemRef* p) : BasePtr(p) {}
		ElemRefPtr(const ElemRefPtr& p) : BasePtr(p) {}
	};

protected:
	typedef ReadWriteLockable<Container> MyBase;
	typedef boost::multi_index::multi_index_container<
				MyElemRef,
				boost::multi_index::indexed_by<
					boost::multi_index::ordered_unique<
						boost::multi_index::const_mem_fun<
							MyElemRef, const value_type*, &MyElemRef::constElemPtr>,
						std::less<const value_type*>
					>
			> >
			RefPool;
	RefPool m_refPool;

protected:
	bool isLockedElement(const value_type& e)
		{ return m_refPool.find(&e) != m_refPool.end();	}

	/**
	 @brief 取得元素的引用

	 @前条件：iter 必须指向容器中的有效元素，不能为 end()

	 @note 调用该函数之前，必须先给 this 加读锁或写锁\n
	  iter  必须是加锁之后从容器中找到的 iterator..\n
	 - only let caller use Interface of ElemRefCounterClass..
	 */
	typename RefPool::iterator getElemRefIter(iterator iter);

	bool tryerase_Impl(ElemRefPtr& pRef);

//protected: // gcc do not greceful support template friend function, declare as public
	friend void intrusive_ptr_add_ref(MyElemRef* p)
	{
		p->container()->addElemRef(p);
	}
	friend void intrusive_ptr_release(MyElemRef* p)
	{
		p->container()->addElemRef(p);
	}
//public:
	void addElemRef(MyElemRef* pRef);

	// must unlock(this) before call this function
	void releaseElemRef(MyElemRef* pRef);

public:
	/**
	 @brief 取得元素的引用

	 @param[out] refPtr 元素的引用将被复制到该参数
	 @param[in]  iter   要引用的元素 iterator
	 - 前条件：refPtr 必须为 NULL，iter 不能为 this-end()
	 @note
		- 调用该函数之前，必须为容器加读锁或写锁
		- 退出 refPtr 的作用域之前，必须为容器解锁

		before calling this function, must read/write lock(this)
		before exit refPtr's scope, must unlock the LockableContainer
		before calling these two function, dest ElemRefPtr must be null
	 */
	void getRefPtr(ElemRefPtr& refPtr, iterator iter)
	{ // more efficient because need not create a temp value on return!
	  // if refPtr's original ptr is not null, old object pointed by its ptr
	  // maybe erased if it was eraseOnRelease.
	  // refPtr's old ptr must be null!!
		assert(refPtr.get() == 0);
		refPtr = const_cast<MyElemRef*>(&*getElemRefIter(iter));
	}

	/**
	 @brief 取得元素的引用

	 @param[in]  iter   要引用的元素 iterator
	 @return  获得的元素引用
	 @note
	   因为调用该函数前必须加写锁，所以
	   这个函数的正确性依赖于 返回值优化，如果编译器没有返回值优化，必须使用
	   void getRefPtr(ElemRefPtr& refPtr, iterator iter)

	   l-value's old ptr must be null!!
	   the best way is to use this function in ElemRefPtr's constructor, like:
	     ElemRefPtr p = container.getRefPtr(iter); // or:
	     ElemRefPtr p(container.getRefPtr(iter));
	 */
	ElemRefPtr getRefPtr(iterator iter)
	{ // if compiler do the optimize on return, it will not create a temp value
	  // when ElemRefPtr p = getRefPtr(iter)!
		return ElemRefPtr(const_cast<MyElemRef*>(&*getElemRefIter(iter)));
	}

	/**
	 @brief 测试该容器是否有元素被引用了
	 */
	bool hasElemRef()
	{
		typename MyBase::ReadLockSentry sentry(*this);
		return !m_refPool.empty();
	}

////////////////////////////////////////////////////////////////////
	// 调用参数为 ElemRefPtr 的函数之前，都必须确保已解锁 this
	// 调用参数为 iterator   的函数之前，都必须对 this 加写锁

	/**
	 @brief 尝试删除

	 @return 成功删除返回 true，失败返回 false
	 @note 调用该函数之前，都必须确保已为容器解锁
	 */
	bool tryErase(ElemRefPtr& pRef);

	/**
	 @brief 尝试删除

	 @return 成功删除返回 true，失败返回 false
	 @note 调用该函数之前，都必须确保已为容器加写锁
	 */
	bool tryErase(iterator iter);


	/**
	 @brief 删除元素

	 @note
	   - 调用该函数之前，都必须确保已为容器解锁
	   - erase 的语义是 eraseOnRelease, 多线程情况下不能实现 erase right now ！
	   - 查看源码中的标记"old.erase"
	 */
	void erase(ElemRefPtr& pRef) { eraseOnRelease(pRef); }

	/**
	 @brief 删除元素

	 @note
	   - 调用该函数之前，都必须确保已为容器加写锁
	   - erase 的语义是 eraseOnRelease, 多线程情况下不能实现 erase right now ！
	   - 查看源码中的标记"old.erase"
	 */
	void erase(iterator iter) { eraseOnRelease(iter); }

	// maybe erase right now, maybe delay erase also
	void eraseOnRelease(iterator iter);
	void eraseOnRelease(ElemRefPtr& pRef);

////////////////////////////////////////////////////////////////////
	// place holder for no default constructor error!
	LockableContainer()	{ }

	/**
	 @brief 删除容器中的所有元素

	 @note 调用该函数之前必须先解锁 this
	 */
	void clear();

	~LockableContainer() { clear(); }
};

///////////////////////////////////////////////////////////////////////////////////////////

template<class Container, class ElemRefCounterClass>
struct LockableTraits<LockableContainer<Container, ElemRefCounterClass> >
{
	typedef ContainerElementLockableTag LockableTag;
};

///////////////////////////////////////////////////////////////////////////////////////////


template<class Container, class ElemRefCounterClass> inline
typename
LockableContainer<Container, ElemRefCounterClass>::RefPool::iterator
LockableContainer<Container, ElemRefCounterClass>::getElemRefIter(iterator iter)
{
	const value_type* pElem = &*iter;
	typename RefPool::iterator iRef = m_refPool.find(pElem);
	if (m_refPool.end() == iRef)
	{
		std::pair<typename RefPool::iterator, bool> ib;
		ib = m_refPool.insert(typename RefPool::value_type(MyElemRef(this, iter)));
		assert(ib.second);
		iRef = ib.first;

		MyElemRef& elemRef = const_cast<MyElemRef&>(*iRef);
		elemRef.construct();
	} else {
		// other thread already locked this element...
	}
	return iRef;
}

template<class Container, class ElemRefCounterClass> inline
void LockableContainer<Container, ElemRefCounterClass>::addElemRef(MyElemRef* pRef)
{
#if defined(_DEBUG) || defined(DEBUG) || !defined(NDEBUG)
	ElemRefCounterClass& vvvvv = pRef->value();
#endif
//	typename MyBase::WriteLockSentry sentry(*this);
	pRef->value().addRef();
}

template<class Container, class ElemRefCounterClass> inline
void LockableContainer<Container, ElemRefCounterClass>::releaseElemRef(MyElemRef* pRef)
{
	typename MyBase::WriteLockSentry sentry(*this);
#if defined(_DEBUG) || defined(DEBUG) || !defined(NDEBUG)
	ElemRefCounterClass& vvvvv = pRef->value();
#endif

	if (0 == pRef->value().releaseRef())
	{
		iterator iter = pRef->iter();
		typename RefPool::iterator iRef = m_refPool.find(pRef->constElemPtr());
		assert(m_refPool.end() != iRef); // must in lock pool

		if (pRef->value().isEraseOnRelease())
		{	// lock for erase!!
		//	typename MyBase::WriteLockSentry sentry(*this);
		//	DEBUG_printf("LockableContainer::releaseElemRef, eraseOnRelease\n");
			Container::erase(iter);
		}
		// destroy the object in object holder before erase!!
		const_cast<MyElemRef&>(*iRef).destroy();
		m_refPool.erase(iRef);
	}
}

template<class Container, class ElemRefCounterClass> inline
bool LockableContainer<Container, ElemRefCounterClass>::tryerase_Impl(ElemRefPtr& pRef)
{
#if defined(_DEBUG) || defined(DEBUG) || !defined(NDEBUG)
	ElemRefCounterClass& vvvvv = pRef->value();
#endif
	const value_type* pElem = &pRef->elem();
	typename RefPool::iterator iRef = m_refPool.find(pElem);

	assert(m_refPool.end() != iRef); // must in lock pool
	assert(pRef->value().getRefCount() >= 1);

	if (1 == pRef->value().getRefCount())
	{
		// only current thread holds the ref!!
		Container::erase(pRef->iter());

		// destroy the object in object holder before erase!!
		const_cast<MyElemRef&>(*iRef).destroy();
		m_refPool.erase(iRef);

		// now *pRef was destroyed and is invalidate, clear intrusive_ptr:pRef
		// this is some ugly...
		// reset pRef's internal saved raw ptr!
		// use null to re-construct ElemRefPtr on pRef's place
		new (&pRef) ElemRefPtr(0);

		return true;
	} else
		return false;
}

template<class Container, class ElemRefCounterClass> inline
bool LockableContainer<Container, ElemRefCounterClass>::tryErase(ElemRefPtr& pRef)
{
	typename MyBase::WriteLockSentry sentry(*this);
	bool bRet = tryerase_Impl(pRef);
	return bRet;
}

template<class Container, class ElemRefCounterClass> inline
bool LockableContainer<Container, ElemRefCounterClass>::tryErase(iterator iter)
{
	typename RefPool::iterator iRef = m_refPool.find(&*iter);
	if (m_refPool.end() == iRef)
	{
		MyBase::erase(iter);
		return true;
	}
	return false;
}

/*
"old.erase"
如果多个线程在同时企图删除同一个元素，这样的 erase 会导致程序陷入死锁，
因为每个线程都在等待别的线程释放 pRef->m_refCount, 自己却不释放 pRef->m_refCount，
而别的线程也在同样等待，如此的死锁.....

template<class Container, class ElemRefCounterClass> inline
void LockableContainer<Container, ElemRefCounterClass>::erase(ElemRefPtr& pRef)
{
	while (true)
	{
		typename MyBase::WriteLockSentry sentry(*this);

		if (tryerase_Impl(pRef)) {
			break;
		} else {
			pRef->value().setErasing(true);
			sentry.release();

			const int timeout = 100 * pRef->value().getRefCount();
			Thread::sleep(timeout);
		}
	}
}

template<class Container, class ElemRefCounterClass> inline
void LockableContainer<Container, ElemRefCounterClass>::erase(iterator iter)
{
	typename RefPool::iterator iRef = m_refPool.find(&*iter);
	if (m_refPool.end() == iRef) {
		MyBase::erase(iter);
	} else {
		ElemRefPtr pElemRef = getRefPtr(iter);
		MyBase::unlock(); // unlock before erase(ElemRefPtr& pRef)
		erase(pElemRef);
		MyBase::writeLock(); // keep lock after erase(ElemRefPtr& pRef)
	}
}
*/

template<class Container, class ElemRefCounterClass> inline
void LockableContainer<Container, ElemRefCounterClass>::eraseOnRelease(iterator iter)
{
	typename RefPool::iterator iRef = m_refPool.find(&*iter);
	if (m_refPool.end() == iRef)
		MyBase::erase(iter);
	else
		const_cast<MyElemRef&>(*iRef).value().setEraseOnRelease(true);
}

template<class Container, class ElemRefCounterClass> inline
void LockableContainer<Container, ElemRefCounterClass>::eraseOnRelease(ElemRefPtr& pRef)
{
	assert(pRef);
	if (!tryErase(pRef))
	{
		pRef->value().setEraseOnRelease(true);
		pRef = (MyElemRef*)(0); // set to null!
	}
}

template<class Container, class ElemRefCounterClass> inline
void LockableContainer<Container, ElemRefCounterClass>::clear()
{
	DEBUG_only(int waitTimes = 0;);
	while (true)
	{
		MyBase::writeLock();
		if (m_refPool.empty()) {
			Container::clear(); // require this method
			MyBase::unlock();
			break;
		} else {
			DEBUG_only(
				if (waitTimes++ % 40 == 0) {
					fprintf(stderr,
						"LockableContainer<Container, ElemRefCounterClass>::clear()\n"
						"Container::value_type=%s, ElemRefCounterClass=%s\n"
						"waitTimes=%d, m_refPool.size=%d, wait for reference to element to be released..\n"
						, typeid(value_type).name(), typeid(ElemRefCounterClass).name()
						, waitTimes, m_refPool.size());
				}
			);
			MyBase::unlock();
			Thread::sleep(50);
		}
	} // while
}
/*
//////////////////////////////////////////////////////////////////////////////////////////////////
template<class LockableContainerType, class ElemRefCounterClass> inline
void intrusive_ptr_add_ref(LockableContainer_ElemRef<LockableContainerType, ElemRefCounterClass>* p)
{
	p->container()->addElemRef(p);
}
template<class LockableContainerType, class ElemRefCounterClass> inline
void intrusive_ptr_release(LockableContainer_ElemRef<LockableContainerType, ElemRefCounterClass>* p)
{
	p->container()->releaseElemRef(p);
}
//////////////////////////////////////////////////////////////////////////////////////////////////
*/

} } // namespace terark::thread

#endif // LockableContainer_h__


// @} end file LockableContainer.hpp


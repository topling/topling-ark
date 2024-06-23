/* vim: set tabstop=4 : */
/********************************************************************
	@file LockSentry.hpp
	@brief 提供各种 LockSentry 哨位对象

	@date	2006-9-28 17:26
	@author	Lei Peng
	@{
*********************************************************************/
#ifndef __terark_thread_LockSentry_h__
#define __terark_thread_LockSentry_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#include <assert.h>
#include <boost/static_assert.hpp>
#include <terark/stdtypes.hpp>

namespace terark { namespace thread {

/**
 @addtogroup thread
 @{
 */

/**
 @defgroup LockSentry LockSentry 哨位对象
 @{
  - 名词：哨位(Sentry) 是指利用 C++ 的“资源申请即初始化”特性，对于需要按特定顺序调用的函数序列。
    构造时调用一个函数，析构时调用另外一个函数，来构建强壮、安全的程序。

  - 使用哨位对象可以避免忘记解锁，异常时锁泄漏等错误。良好的程序应该使用哨位。

  - 该模块提供了用于自动守卫对锁对象的哨位。

  - 哨位的典型用法如下：

  @code
	{// make a scope
		SomeLockSentry sentry(lockableObject);
		// do something...
	}
  @endcode

  - 从理论上讲，所有需要加锁的代码块，都可以仅用哨位就可以实现。\n
	因此，boost::thread 中的锁对象甚至只允许使用 sentry，称作 scoped_lock.

  - 本模块实现的哨位
	- 智能自动哨位【根据被守卫的对象类型，自动推导合适的哨位类型并实例化，非常便于使用】
		@see LockSentry, MutexLockSentry, ReadLockSentry, WriteLockSentry
	- 固定哨位【只能用于一种类型的哨位，会产生更好的执行代码】
		@see MutexLockSentryable, ReadLockSentryable, WriteLockSentryable
 @}
 */
//@}

/**
 @ingroup LockSentry
 @brief 自动推导被 sentry 的对象类型并生成正确的 sentry 对象

  使用传入的参数类型，通过类型推导，生成一个代理对象，该代理对象是 LockProxyInterface
  的子类对象，它只占两个指针的空间，因此把它放在 m_lockProxyBuf 中，手工使用 placement new
  来构造该代理对象，就不用使用 new-delete，从而减少了开销。所有的额外开销都只在虚函数调用上。

 @note 这个类只是MutexLockSentry/ReadLockSentry/WriteLockSentry 的公共基类，仅在 terark 库内部使用
 @note 用户程序不能创建这个类的对象 can not create a 'LockSentry' by user code!
 */
class LockSentry
{
protected:
	/**
	 @brief 代理锁对象的抽象基类
	 */
	class LockProxyInterface
	{
	public:
		virtual void lock() = 0;
		virtual void unlock() = 0;
	};

protected:
	char m_lockProxyBuf[2 * sizeof(void*)]; ///< 保存 LockProxyInterface 的子类对象的空间
	bool m_isLocked; ///< 是否已锁定，如果已锁，析构时会通过 LockProxyInterface 接口调用 unlock

protected:
	/**
	 @brief sentry 的核心实现函数

	 @param xlock 要被自动锁定的对象
	 @param tag   未用，仅用于类型推导
	 */
	template<class LockableType, class LockProxyType>
	void sentry_imp(LockableType& xlock, LockProxyType* tag)
	{
		// @note if this STATIC_ASSERT failed, make m_lockProxyBuf a little bigger!!
		BOOST_STATIC_ASSERT(sizeof(m_lockProxyBuf) >= sizeof(LockProxyType));
		assert(!m_isLocked);
		new(m_lockProxyBuf)LockProxyType(xlock);
		LockProxyInterface* plock = (LockProxyInterface*)m_lockProxyBuf;
		plock->lock(); // maybe throw exceptions..

		// @note must at the end, for exception safe!!
		m_isLocked = true;
	}

	LockSentry() { m_isLocked = false; } // only can be used by derived class
	~LockSentry()
	{
		if (m_isLocked) {
			// @note 此时并不知道 xlock 的具体类型，只知道它有 LockProxyInterface 接口
			LockProxyInterface* xlock = (LockProxyInterface*)m_lockProxyBuf;
			xlock->unlock();
		}
	}

public:
	/**
	 @brief 手工释放该对象的锁

	  从理论上讲，该函数不是必须的，所有使用了该函数的代码，
	  都可以重写为不需要调用该函数的另外一种形式。

     @note 不推荐使用该函数
	 */
	void release()
	{
		assert(m_isLocked);
		LockProxyInterface* xlock = (LockProxyInterface*)m_lockProxyBuf;
		xlock->unlock();
		m_isLocked = false;
	}

	/**
	 @brief 判断该对象是否已加锁

	 @note 不建议使用该函数
	 */
	bool isLocked() const throw() { return m_isLocked; }
};

/**
 @ingroup LockSentry
 @brief 互斥锁的哨位对象

  用于可以被加互斥锁的对象
  can be used for 'DummyLockable' also!
 */
class MutexLockSentry : public LockSentry
{
	DECLARE_NONE_COPYABLE_CLASS(MutexLockSentry)

private:
	/**
	 @brief 具体互斥锁对象的代理

	 @param MutexLockable 可被互斥锁的对象类型
	  将 MutexLockable::lock/unlock 代理到自身的 lock/unlock
	 */
	template<class MutexLockable>
	class LockProxy : public LockSentry::LockProxyInterface
	{
		MutexLockable* m_xlock;
	public:
		LockProxy(MutexLockable& xlock) : m_xlock(&xlock) {}

		void lock()   { m_xlock->lock();   }
		void unlock() { m_xlock->unlock(); }
	};

public:
	MutexLockSentry() { }

	/**
	 @brief 构造函数
	 @param xlock 要被守卫的对象

	 通过对象类型自动推导并实例化出模板类 LockProxy<MutexLockable>，再实例化这么一个对象
	 */
	template<class MutexLockable>
	MutexLockSentry(MutexLockable& xlock) { sentry(xlock); }

	/**
	 @brief 提供同一个守卫对象可以多次守卫其它对象的功能

	 @param xlock 要被守卫的对象

	 - 通过对象类型自动推导并实例化出模板类 LockProxy<MutexLockable>，再实例化这么一个对象

	 @note 不建议使用
	 */
	template<class MutexLockable>
	void sentry(MutexLockable& xlock)
	{
		sentry_imp(xlock, (LockProxy<MutexLockable>*)(0));
	}
};

/**
 @ingroup LockSentry
 @brief 读锁的哨位对象

  用于可以被加读锁的对象
 */
class ReadLockSentry : public LockSentry
{
	DECLARE_NONE_COPYABLE_CLASS(ReadLockSentry)

private:
	/**
	 @brief 具体互斥锁对象的代理

	 @param ReadLockable 可被互斥锁的对象类型
	  将 ReadLockable::readLock/unlock 代理到自身的 lock/unlock
	 */
	template<class ReadLockable>
	class LockProxy : public LockSentry::LockProxyInterface
	{
		ReadLockable* m_xlock;
	public:
		LockProxy(ReadLockable& xlock) : m_xlock(&xlock) {}

		void lock()   { m_xlock->readLock(); }
		void unlock() { m_xlock->unlock(); }
	};

public:
	ReadLockSentry() { }

	/**
	 @brief 构造函数
	 @param xlock 要被守卫的对象

	 通过对象类型自动推导并实例化出模板类 LockProxy<ReadLockable>，再实例化这么一个对象
	 */
	template<class ReadLockable>
	ReadLockSentry(ReadLockable& xlock) { sentry(xlock); }

	/**
	 @brief 提供同一个守卫对象可以多次守卫其它对象的功能

	 @param xlock 要被守卫的对象

	 - 通过对象类型自动推导并实例化出模板类 LockProxy<ReadLockable>，再实例化这么一个对象

	 @note 不建议使用
	 */
	template<class ReadLockable>
	void sentry(ReadLockable& xlock)
	{
		sentry_imp(xlock, (LockProxy<ReadLockable>*)(0));
	}
};

/**
 @ingroup LockSentry
 @brief 写锁的哨位对象

  用于可以被加写锁的对象
 */
class WriteLockSentry : public LockSentry
{
	DECLARE_NONE_COPYABLE_CLASS(WriteLockSentry)

private:
	/**
	 @brief 具体互斥锁对象的代理

	 @param WriteLockable 可被互斥锁的对象类型
	  将 WriteLockable::writeLock/unlock 代理到自身的 lock/unlock
	 */
	template<class WriteLockable>
	class LockProxy : public LockSentry::LockProxyInterface
	{
		WriteLockable* m_xlock;
	public:
		LockProxy(WriteLockable& xlock) : m_xlock(&xlock) {}

		void lock()   { m_xlock->writeLock(); }
		void unlock() { m_xlock->unlock(); }
	};

public:
	WriteLockSentry() { }

	/**
	 @brief 构造函数
	 @param xlock 要被守卫的对象

	 通过对象类型自动推导并实例化出模板类 LockProxy<WriteLockable>，再实例化这么一个对象
	 */
	template<class WriteLockable>
	WriteLockSentry(WriteLockable& xlock) { sentry(xlock); }

	/**
	 @brief 提供同一个守卫对象可以多次守卫其它对象的功能

	 @param xlock 要被守卫的对象

	 - 通过对象类型自动推导并实例化出模板类 LockProxy<WriteLockable>，再实例化这么一个对象

	 @note 不建议使用
	 */
	template<class WriteLockable>
	void sentry(WriteLockable& xlock)
	{
		sentry_imp(xlock, (LockProxy<WriteLockable>*)(0));
	}
};

//////////////////////////////////////////////////////////////////

/**
 @ingroup LockSentry
 @brief 提供仅能守卫 MutexLockableClass 的功能

  - 比自动的 MutexLockSentry 的好处在于减少了虚函数调用，并且减少了代码体积。
  - 因为自动的 MutexLockSentry 几乎一定会生成代理类的代码和虚函数表，而这个类模板通过编译器优化，
    都转化为对 MutexLockableClass 成员函数的直接调用了。
  - 因此，使用这个类模板比自动的 MutexLockSentry 更高效
  - 在 MutexLockableClass 内部使用一个
  @code typedef MutexLockSentryable<MutexLockableClass> MutexLockSentry; @endcode
    可以很方便地使用
  @code MutexLockableClass::MutexLockSentry sentry(object); @endcode
 */
template<class MutexLockableClass>
class MutexLockSentryable
{
	MutexLockableClass& m_xobj;
public:
	MutexLockSentryable(MutexLockableClass& xobj)
	   	: m_xobj(xobj) { xobj.lock(); }
	~MutexLockSentryable() { m_xobj.unlock(); }
};

/**
 @ingroup LockSentry
 @brief 类似 MutexLockSentryable
 @see MutexLockSentryable
 */
template<class ReadLockableClass>
class ReadLockSentryable
{
	ReadLockableClass& m_xobj;
public:
	ReadLockSentryable(ReadLockableClass& xobj)
	   	: m_xobj(xobj) { xobj.readLock(); }
	~ReadLockSentryable() { m_xobj.unlock(); }
};

/**
 @ingroup LockSentry
 @brief 类似 MutexLockSentryable
 @see MutexLockSentryable
 */
template<class WriteLockableClass>
class WriteLockSentryable
{
	WriteLockableClass& m_xobj;
public:
	WriteLockSentryable(WriteLockableClass& xobj)
	   	: m_xobj(xobj) { xobj.writeLock(); }
	~WriteLockSentryable() { m_xobj.unlock(); }
};

} } // namespace terark::thread

#endif // __terark_thread_LockSentry_h__

// @} end file LockSentry.hpp


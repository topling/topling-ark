/* vim: set tabstop=4 : */
/********************************************************************
	@file thread.hpp
	@brief 线程对象

	@date	2007-03-16 14:45
	@author	Lei Peng
	@{
*********************************************************************/
#ifndef __terark_thread_base_h__
#define __terark_thread_base_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#include <boost/intrusive_ptr.hpp>
#include <terark/util/refcount.hpp>
#include "SynchObject.hpp"

namespace terark { namespace thread {

/**
 @brief 线程基础类

  抽象基类，仅为内部实现使用，用户不能使用该类
 */
class TERARK_DLL_EXPORT ThreadBase : public RefCounter
{
private:
	ThreadBase(const ThreadBase& rhs);
	ThreadBase& operator=(const ThreadBase& rhs);

public:
	enum StateType {
		state_Initial,
		state_Ready,
		state_Running,
		state_Completed
	};

protected:
	volatile mutable int m_exitCode;
	volatile mutable StateType m_state;
	volatile mutable std::exception* m_exception;

protected:
	ThreadBase();
	virtual ~ThreadBase();

	/**
	 @brief thread execution method

	  return value will saved in m_exitCode

     @note gdb 不能显示抽象基类（有纯虚函数的类）的内容，所以在 debug 时不使用抽象基类
	 */
#if defined(_DEBUG) || !defined(NDEBUG)
	virtual void run() { assert(0); }
#else
	virtual void run() = 0;
#endif

	/**
	 @brief this method will be called after run() return

	  you can destroy 'this' in onComplete, if you do this:
	  -# you can not call any method after thread exit, such as exitCode()/join()/state(),
	  -# in practice, you should not call any method after start().
	 */
	virtual void onComplete();

    //! @note gdb 不能显示抽象基类（有纯虚函数的类）的内容，所以在 debug 时不使用抽象基类
#if defined(_DEBUG) || !defined(NDEBUG)
	virtual void do_start(int stackSize) {}
	virtual void do_join() {}
#else
	virtual void do_start(int stackSize) = 0;
	virtual void do_join() = 0;
#endif

	void threadMain();
	static void threadProcImpl(void* param);

public:
	int exitCode();
	void start(int stackSize = 0);
	void join();
	StateType state() { return m_state; }
	bool isRunning() { return state_Running == m_state; }
	bool completed() { return state_Completed == m_state; }
};

} } // namespace terark::thread

#endif // __terark_thread_base_h__

// @} end file thread.hpp


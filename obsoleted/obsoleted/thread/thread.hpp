/* vim: set tabstop=4 : */
/********************************************************************
	@file thread.hpp
	@brief 线程对象

	@date	2006-9-30 14:45
	@author	Lei Peng
	@{
*********************************************************************/
#ifndef __terark_thread_h__
#define __terark_thread_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#include "thread_base.hpp"

#if defined(_WIN32) || defined(WIN32)
#	include "win32/thread_impl.hpp"
#else
#	include "posix/thread_impl.hpp"
#endif

#include <boost/function.hpp>

namespace terark { namespace thread {

/**
 @ingroup ThreadObject
 @brief 从一个普通的类成员生成一个线程类

  将一个普通的类成员函数代理给一个线程，如果一个类有多个不同的线程，使用这个类模板非常方便
  @code
  class MyClass
  {
    private:
	 void threadProc1()
	 {
	   // do something...
	 }
	 void threadProc2()
	 {
	   // do something...
	 }
	 MemberFunThread<MyClass, &MyClass::threadProc1> thread1;
	 MemberFunThread<MyClass, &MyClass::threadProc2> thread2;
	public:
	 void fun()
	 {
	   thread1.startWith(this);
	   thread2.startWith(this);

	   // do something...

	 }
  };
  @endcode
 */
/**
 @ingroup ThreadObject
 @brief 类模板 MemberFunThread 的部分特化

 仅对成员函数返回值为 void 的特化，该特化无 getReturnValue 成员函数
 */
template<class Class, void (Class::*PtrToMemberFunction)()>
class MemberFunThread : public Thread
{
	Class* m_object;
public:
	explicit MemberFunThread(Class* pObject) : m_object(pObject) { }
	Class* getObject() const { return m_object; }
protected:
	void run()
	{
		(m_object->*PtrToMemberFunction)();
	}
};

class BindThread : public Thread
{
	boost::function0<void> m_func;
public:
	explicit BindThread(const boost::function0<void>& func) : m_func(func) {}
protected:
	void run()
	{
		m_func();
	}
};

}

} // namespace terark::thread

#endif // __terark_thread_h__

// @} end file thread.hpp


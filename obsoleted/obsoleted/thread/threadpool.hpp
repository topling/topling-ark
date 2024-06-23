/* vim: set tabstop=4 : */
#ifndef __threadpool_h__
#define __threadpool_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
# pragma warning(push)
# pragma warning(disable: 4018)
# pragma warning(disable: 4267)
#endif

#include <terark/stdtypes.hpp>
#include <set>
#include <vector>
#include <deque>

#include "../thread/concurrent_queue.hpp"
#include "../thread/thread.hpp"
//#include "../thread/LockSentry.hpp"
#include <boost/thread/mutex.hpp>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <string>
//#include <boost/multi_index/hashed_index.hpp>
//#include <boost/multi_index/identity.hpp>
//#include <boost/multi_index/sequenced_index.hpp>

namespace terark { namespace thread {

/**
 @addtogroup thread
 @{
 @defgroup ThreadPool 线程池

 @par 提供了两种线程池
  @see ThreadPool 普通的线程池
  @see KeyThreadPool 按对象的键值，有相同键值的任务对象可以按加入线程池的次序，依次顺序处理

 @par 用法
 @code
	struct MyTask
	{
		MyTask(const std::string& message) : message(message) {}
		std::string message;
	};
	class MyThreadPool : public ThreadPool<MyTask>
	{
	protected:
		void processTask(MyTask& task)
		{
			std::cout << task.message << std::endl;
		}
	};
	class MyKeyThreadPool : public KeyThreadPool<int, MyTask>
	{
	protected:
		void processTask(const int& key, MyTask& task)
		{
			std::cout << "key=" << key ", message=" << task.message << std::endl;
		}
	};
	///
	void main()
	{
		MyThreadPool tp(
			3,   // maxThreads
			100  // maxQueueSizePerThread
			);
		tp.addTask("11111");
		tp.addTask("22222");
		tp.addTask("33333");

		MyKeyThreadPool ktp(
			10, // maxThreads
			20  // maxQueueSizePerThread
			);
		ktp.addTask(1, "11111");
		ktp.addTask(2, "22222");
		ktp.addTask(3, "33333");

		tp.complete();
		ktp.complete();
	}
 @endcode

 @par 有时候使用 MemberFunThreadPool 和 MemberFunKeyThreadPool 更方便
 */
//@}
//@}

class TERARK_DLL_EXPORT ThreadPoolBase; // forward declaration

/**
 @brief 工作线程基类，仅用于内部实现

 提供线程池的工作线程的基本功能
 */
class TERARK_DLL_EXPORT WorkThreadBase : public Thread
{
	DECLARE_NONE_COPYABLE_CLASS(WorkThreadBase)

public:
	volatile bool running;
	int inputTaskCount;
	ThreadPoolBase* m_owner;

	WorkThreadBase(ThreadPoolBase* owner);

	void stop()	{ running = false; }
	bool isRunning() const { return running; }

	void checkForAddTask();

	virtual int taskQueueSize() = 0; // locked get queueSize
};

/**
 @brief 线程池基类，仅用于内部实现

 提供线程池的基本功能
 */
class TERARK_DLL_EXPORT ThreadPoolBase
{
	DECLARE_NONE_COPYABLE_CLASS(ThreadPoolBase)

public:
	int maxThreads() const throw() { return m_maxThreads; }
	int maxQueueSizePerThread() const throw() { return m_maxQueueSizePerThread; }
	int taskQueuePopTimeout() const throw() { return m_taskQueuePopTimeout; }
	int totalInputTaskCount() const throw();
	int totalProcessedTaskCount() const;

	void init(int maxThreads, int maxQueueSizePerThread, int taskQueuePopTimeout = 500);

	// this function maybe cause some task will be discarded!
	void stop();

	// stop after all thread's task queue are empty!!
	// this function is safe, no any task will be discarded..
	void complete();

//protected:
public:
	boost::mutex m_mutex;
protected:
	int m_maxQueueSizePerThread;
	int m_maxThreads;
	int m_taskQueuePopTimeout; // in milliseconds

//protected:
public:
	typedef concurrent_queue<std::set<WorkThreadBase*> >  FreeThreadSet;
	FreeThreadSet m_freeThreads;
	std::vector<WorkThreadBase*> m_threads;

	enum StateType
	{
		state_Initial,
		state_Running,
		state_Stopping,
		state_Stopped
	};
	volatile StateType m_state;

protected:
	ThreadPoolBase(int taskQueuePopTimeout);
	virtual ~ThreadPoolBase();

protected:
	virtual WorkThreadBase* newThread() = 0;

	WorkThreadBase* selectMinQueueThread(int* minSize) const;
	WorkThreadBase* getAvailableThread();
};

//////////////////////////////////////////////////////////////////////////

template<class Task> class ThreadPool; // forward declaration

/**
 @brief ThreadPool 的工作线程池
 */
template<class Task>
class WorkThread : public WorkThreadBase
{
	DECLARE_NONE_COPYABLE_CLASS(WorkThread)
	friend class ThreadPool<Task>;

protected:
	typedef concurrent_queue<std::set<WorkThread*> >  FreeThreadSet;
	typedef concurrent_queue<std::deque<Task> > TaskQueue;
	typedef ThreadPool<Task> thread_pool;

	TaskQueue taskQueue;

	WorkThread(int maxQueueSize, thread_pool* owner)
		: taskQueue(maxQueueSize), WorkThreadBase(owner) {}

	void addTask(const Task& task)
	{
		checkForAddTask();
		taskQueue.push_back(task);
	}

	// locked get queueSize
	virtual int taskQueueSize() { return taskQueue.size(); }

	virtual void run();
};

/**
 @ingroup ThreadPool
 @brief ThreadPool 普通的线程池
 @note
// before addTask:
// must ThreadPool::ThreadPool(int maxThreads, int maxQueueSizePerThread)
//   or call
// ThreadPool::ThreadPool() and call
// ThreadPool::init(int maxThreads, int maxQueueSizePerThread)
 */
template<class Task>
class ThreadPool : public ThreadPoolBase
{
	DECLARE_NONE_COPYABLE_CLASS(ThreadPool)
	friend class WorkThread<Task>;

public:
	typedef WorkThread<Task> thread_type;
	typedef typename thread_type::FreeThreadSet FreeThreadSet;

protected:
	/**
	 @brief 处理任务

	 @note user code must override this function
	 */
	virtual void processTask(Task& task) = 0;

	// implement..
	WorkThreadBase* newThread()
	{ return new thread_type(m_maxQueueSizePerThread, this); }

public:
	ThreadPool(int taskQueuePopTimeout = 1000) : ThreadPoolBase(taskQueuePopTimeout)
	{
	}
	ThreadPool(int maxThreads, int maxQueueSizePerThread, int taskQueuePopTimeout = 1000)
		: ThreadPoolBase(maxThreads, maxQueueSizePerThread, taskQueuePopTimeout)
	{
	}

	/**
	 @brief add a task to ThreadPool's queue

	 can be called in multi-thread...
	 */
	void addTask(const Task& task)
	{
		thread_type* pThread = static_cast<thread_type*>(getAvailableThread());
		assert(dynamic_cast<thread_type*>(pThread) != NULL);
		pThread->addTask(task); // maybe waiting
	}
//	void start(); // do not needed, threads will auto start in addTask()..
};

template<class Task> void WorkThread<Task>::run()
{
	m_owner->m_state = thread_pool::state_Running;
	running = true;
	while (running)
	{
		if (m_owner->m_freeThreads.peekEmpty()) // non locked test
		{
			// other thread maybe waiting for addTask().
			if (!taskQueue.full())
				// now this thread's taskQueue is ready for put.
				(void)m_owner->m_freeThreads.insert_unique(this); // should not wait!!
		} else {
			if (taskQueue.peekEmpty())
				(void)m_owner->m_freeThreads.insert_unique(this); // should not wait!!
		}
		Task task;
		if (taskQueue.pop_front(task, m_owner->taskQueuePopTimeout()))
		{
			((thread_pool*)m_owner)->processTask(task);
		}
	}
	if (!taskQueue.empty())
	{
		DEBUG_only(
			fprintf(stderr, "%s: taskQueue not empty, some task will be discarded..\n",
				BOOST_CURRENT_FUNCTION);
		);
		taskQueue.clearQueue();
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template<class Key, class Task> class KeyThreadPool; //!< forward declaration

/**
 @brief KeyThreadPool 的工作线程，仅用于内部实现
 */
template<class Key, class Task>
class KeyWorkThread : public WorkThreadBase
{
	DECLARE_NONE_COPYABLE_CLASS(KeyWorkThread)
	friend class KeyThreadPool<Key, Task>;

public:
	typedef Key key_type;
	typedef KeyThreadPool<Key, Task> thread_pool;
	typedef concurrent_queue<std::deque<Task> > TaskQueue;

protected:
	key_type* volatile key; // current key bind to the thread!!
	TaskQueue taskQueue;

	thread_pool* owner() const { return static_cast<thread_pool*>(m_owner); }
	int taskQueueSize()  { return taskQueue.size(); }

//public:
	KeyWorkThread(int maxQueueSize, thread_pool* owner)
		: WorkThreadBase(owner), key(0) {}

	bool doAddTask(const key_type& key, const Task& task);
	bool addTask(const key_type& key, const Task& task);
	virtual void run(); // override
};

/**
 @ingroup ThreadPool
 @brief 把跟某个键值关联的所有对象放入同一个线程处理，以便对这一类对象的操作序列化

 @note
  void processTask(key_type& key, Task&);  or equivalent--\n
    --this function must not modify key-data in (key_type& key)

 @note
  before addTask:
   must KeyThreadPool::KeyThreadPool(int maxThreads, int maxQueueSizePerThread)
     or call
   KeyThreadPool::KeyThreadPool() and call
   KeyThreadPool::init(int maxThreads, int maxQueueSizePerThread)
 */
template<class Key, class Task>
class KeyThreadPool : public ThreadPoolBase
{
	DECLARE_NONE_COPYABLE_CLASS(KeyThreadPool)
	friend class KeyWorkThread<Key, Task>;

protected:
	// this function can not alter the key-part of 'key'.
	virtual void processTask(const Key& key, Task& task) = 0;

public:
	typedef Key key_type;
	typedef KeyWorkThread<Key, Task> thread_type;

protected:
	struct Element
	{
		key_type m_key;
		thread_type* m_thread;

		Element(const key_type& key, thread_type* pthread)
			: m_key(key), m_thread(pthread)
		{ }
	};
//	struct TagThread{};
	typedef boost::multi_index::multi_index_container<
	  Element,
	  boost::multi_index::indexed_by<
		boost::multi_index::ordered_unique<
	//		boost::multi_index::tag<TagKey>,
			boost::multi_index::member<Element, key_type, &Element::m_key> >,
		boost::multi_index::ordered_unique<
	//		boost::multi_index::tag<TagThread>,
			boost::multi_index::member<Element, thread_type*, &Element::m_thread>,
			std::less<thread_type*> >
	  >
	> MapKeyToThread;
//	typedef typename MapKeyToThread::template index<TagKey   >::type    KeyIndex;
//	typedef typename MapKeyToThread::template index<TagThread>::type ThreadIndex;
//	typedef typename MapKeyToThread::template index<0>::type    KeyIndex;
//	typedef typename MapKeyToThread::template index<1>::type ThreadIndex;
	typedef typename boost::multi_index::nth_index<MapKeyToThread, 0>::type    KeyIndex;
	typedef typename boost::multi_index::nth_index<MapKeyToThread, 1>::type ThreadIndex;
	MapKeyToThread mapKeyToThread;

	KeyIndex& keyIndex() throw() {
//		return mapKeyToThread.template get<0>();
		return boost::multi_index::get<0>(mapKeyToThread);
	}
	ThreadIndex& threadIndex() throw() {
//		return mapKeyToThread.template get<1>();
		return boost::multi_index::get<1>(mapKeyToThread);
	}

private:
	void insertKeyThread(const key_type& key, thread_type* pThread);
	thread_type* newKeyThread(const key_type& key);

	// implement
	WorkThreadBase* newThread() { assert(0); return 0; }

	thread_type* volatile m_pThreadAddingTask;
	bool doAddTask(const key_type& key, const Task& task);

public:
	KeyThreadPool(int taskQueuePopTimeout = 1000)
		: ThreadPoolBase(taskQueuePopTimeout)
	{
		m_pThreadAddingTask = 0;
		DEBUG_only(freqFound = 0;)
	}
	KeyThreadPool(int maxThreads, int maxQueueSizePerThread, int taskQueuePopTimeout = 1000)
		: ThreadPoolBase(maxThreads, maxQueueSizePerThread, taskQueuePopTimeout)
	{
		m_pThreadAddingTask = 0;
		DEBUG_only(freqFound = 0;)
	}
#if defined(_DEBUG) || defined(DEBUG) || !defined(NDEBUG)
	int freqFound;
#endif

	/**
	 @brief add a task to ThreadPool's queue

	 can be called in multi-thread...
	 */
	void addTask(const key_type& key, const Task& task);
};

template<class Key, class Task> inline
void KeyThreadPool<Key, Task>::insertKeyThread(const Key& key, thread_type* pThread)
{
	std::pair<typename MapKeyToThread::iterator, bool>
		ib = mapKeyToThread.insert(Element(key, pThread));
#if defined(_DEBUG) || defined(DEBUG) || !defined(NDEBUG)
	if (!ib.second) {
		int forDebug = 0;
	}
#endif
	pThread->key = &const_cast<Element&>(*ib.first).m_key;
}

template<class Key, class Task> inline
KeyWorkThread<Key, Task>* KeyThreadPool<Key, Task>::newKeyThread(const Key& key)
{
	thread_type* pThread = new thread_type(this->m_maxQueueSizePerThread, this);
	m_threads.push_back(pThread);
	insertKeyThread(key, pThread);
	pThread->start();
	return pThread;
}

template<class Key, class Task> inline
bool KeyThreadPool<Key, Task>::doAddTask(const Key& key, const Task& task)
{
	thread_type* pThread = 0;
	{
		boost::mutex::scoped_lock lock(m_mutex);
		typename KeyIndex::iterator iter = keyIndex().find(key);
		if (keyIndex().end() == iter)
		{
			if (m_threads.size() < m_maxThreads)
				pThread = newKeyThread(key);
		} else {
			pThread = (*iter).m_thread;
			DEBUG_only(freqFound++;)
		}
	}
	if (0 == pThread) // maybe wait
		pThread = static_cast<thread_type*>(m_freeThreads.remove_begin_elem());

	m_pThreadAddingTask = pThread;

	// must unlock m_mutex before addTask!!
	bool bRet = pThread->addTask(key, task); // maybe waiting

	m_pThreadAddingTask = 0;

	return bRet;
}

template<class Key, class Task> inline
void KeyThreadPool<Key, Task>::addTask(const Key& key, const Task& task)
{
	assert(0 != this->m_maxThreads);
	assert(0 != this->m_maxQueueSizePerThread);
	assert(	state_Initial == this->m_state ||
			state_Running == this->m_state );

	while (!doAddTask(key, task))
	{
		// do nothing...
		DEBUG_only(int onlyForDebug = 0;)
	}
#if defined(_DEBUG) || defined(DEBUG) || !defined(NDEBUG)
	int total = totalInputTaskCount();
	double notFoundRatio = (double)(total - freqFound) / total;
#endif
}

//////////////////////////////////////////////////////////////////////////

template<class Key, class Task> inline
bool KeyWorkThread<Key, Task>::doAddTask(const Key& key, const Task& task)
{
	taskQueue.semForPut().wait();

	bool isOK;
	{
		boost::mutex::scoped_lock lock(owner()->m_mutex); // must lock m_mutex
		typename TaskQueue::MutexLockSentry sTaskQueue(taskQueue);
		ThreadPoolBase::FreeThreadSet::MutexLockSentry sFreeThreads(owner()->m_freeThreads);

		if (owner()->m_freeThreads.queue().find(this) ==
			owner()->m_freeThreads.queue().end())
		{
			owner()->insertKeyThread(key, this);
			taskQueue.queue().push_back(task);
			isOK = true;
		}
		else // this is very rare, less than 0.0001%, but it will happen sure!!
			isOK = false;
	}
	if (isOK)
		taskQueue.semForGet().post();
	else
		taskQueue.semForPut().post();

	return isOK;
}

template<class Key, class Task> inline
bool KeyWorkThread<Key, Task>::addTask(const Key& key, const Task& task)
{
	checkForAddTask();

	if (doAddTask(key, task)) {
		inputTaskCount++;
		return true;
	} else
		return false;
}

template<class Key, class Task> inline
void KeyWorkThread<Key, Task>::run() // override
{
	DEBUG_only(bool wasPutToFree = false;)
	owner()->m_state = thread_pool::state_Running;
	running = true;
	while (running)
	{
		if (this != owner()->m_pThreadAddingTask &&
			taskQueue.peekEmpty()) // non locked test!!
		{
			boost::mutex::scoped_lock lock(owner()->m_mutex);

			typename thread_pool::ThreadIndex::iterator
				iter = owner()->threadIndex().find(this);
			if (owner()->threadIndex().end() != iter
				&& taskQueue.peekEmpty()) // test empty() again..
			{
				// My taskQueue was empty but I still lies in mapKeyThread
				owner()->threadIndex().erase(iter);
				this->key = 0;
				(void)owner()->m_freeThreads.insert_unique(this); // should not wait!!
				DEBUG_only(wasPutToFree = true;)
			}
		}

		// once 'this' was put to 'freeThreads', it will waiting here...
		Task task;
		if (taskQueue.pop_front(task, owner()->taskQueuePopTimeout()))
		{
			assert(this->key);
			owner()->processTask(*key, task);
		}
	}
	if (!taskQueue.empty())
	{
		DEBUG_only(
			fprintf(stderr, "%s: taskQueue not empty, some task will be discarded..\n",
				BOOST_CURRENT_FUNCTION);
		);
		taskQueue.clearQueue();
	}
}

//////////////////////////////////////////////////////////////////////////

/**
 @ingroup ThreadPool
 @brief 如果一个类要扮演多种 ThreadPool 功能，可以使用这两个类　MemberFunThreadPool 和 MemberFunKeyThreadPool

 @par 使用示例：
 @code
	struct MyTask
	{
		MyTask(const std::string& message) : message(message) {}
		std::string message;
	};
	class MyClass
	{
		MemberFunThreadPool<MyClass, MyTask, &MyClass::process1> tp1;
		MemberFunKeyThreadPool<MyClass, int, MyKeyTask, &MyClass::process2> tp2;

		void process1(MyTask& task)
		{
			std::cout << task.message << std::endl;
		}

		void process2(const int& key, MyTask& task)
		{
			std::cout << "key=" << key << ", message=" << task.message << std::endl;
		}
	public:
		void fun()
		{
		  tp1.init(3, 10);
		  tp2.init(10, 3);

		  tp1.addTask("11111");
		  tp2.addTask(1, "11111");

		  tp1.complete();
		  tp2.complete();
		}
	};
 @endcode
 @see MemberFunThreadPool, MemberKeyFunThreadPool
 */
template<class Class, class Task, void (Class::*PtrToMemberFunction)(Task&)>
class MemberFunThreadPool : public ThreadPool<Task>
{
	Class* m_object;
public:
	MemberFunThreadPool(Class* pObject = 0)	: m_object(pObject)	{ }
	void setObject(Class* pObject) { m_object = pObject; }
	Class* getObject() const { return m_object; }
protected:
	/**
	 @brief delegate to Class::*PtrToMemberFunction
	 */
	void processTask(Task& x)
	{
		(m_object->*PtrToMemberFunction)(x);
	}
};

/**
 @ingroup ThreadPool
 @brief @see MemberFunThreadPool
 */
template<class Class, class Key, class Task, void (Class::*PtrToMemberFunction)(const Key&, Task&)>
class MemberFunKeyThreadPool : public KeyThreadPool<Key, Task>
{
	Class* m_object;
public:
	MemberFunKeyThreadPool(Class* pObject = 0) : m_object(pObject) { }
	void setObject(Class* pObject) { m_object = pObject; }
	Class* getObject() const { return m_object; }
protected:
	/**
	 @brief delegate to Class::*PtrToMemberFunction
	 */
	void processTask(const Key& key, Task& x)
	{
		(m_object->*PtrToMemberFunction)(key, x);
	}
};

} } // namespace terark::thread

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma warning(pop)
#endif

#endif // __threadpool_h__


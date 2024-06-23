/* vim: set tabstop=4 : */
#include "threadpool.hpp"
#include <typeinfo>

namespace terark { namespace thread {

WorkThreadBase::WorkThreadBase(ThreadPoolBase* owner)
{
	m_owner = owner;
	inputTaskCount = 0;
	running = false;
}

void WorkThreadBase::checkForAddTask()
{
	switch (m_state)
	{
	default: {
		std::string text = "in: ";
		text = text + typeid(*this).name() + "::checkForAddTask(), invalid thread state.";
		DEBUG_printf("%s\n", text.c_str());
		throw ThreadException(text.c_str());
		break; }
	case state_Initial:
		DEBUG_printf(
			"in %s::checkForAddTask(), thread is initial and not running.\n"
			"addTask maybe full fill taskQueue and cause infinite wait.\n",
			typeid(*this).name());
	//	break; // fall through!!...
	case state_Ready:
	case state_Running:
		// ok!!
		break;
	case state_Completed: {
		std::string text = "in: ";
		text = text + typeid(*this).name() + "::checkForAddTask(), thread was exited.";
		DEBUG_printf("%s\n", text.c_str());
		throw ThreadException(text.c_str());
		break; }
	}
}

//////////////////////////////////////////////////////////////////////////

int ThreadPoolBase::totalInputTaskCount() const throw()
{
	int count = 0;
	std::vector<WorkThreadBase*>::const_iterator i = m_threads.begin();
	for (; i != m_threads.end(); ++i)
	{
		count += (*i)->inputTaskCount;
	}
	return count;
}

int ThreadPoolBase::totalProcessedTaskCount() const
{
	int count = totalInputTaskCount();
	std::vector<WorkThreadBase*>::const_iterator i = m_threads.begin();
	for (; i != m_threads.end(); ++i)
	{
		count -= (*i)->taskQueueSize();
	}
	return count;
}

void ThreadPoolBase::init(int maxThreads, int maxQueueSizePerThread, int taskQueuePopTimeout)
{
	m_taskQueuePopTimeout = taskQueuePopTimeout;
	m_maxThreads = maxThreads;
	m_maxQueueSizePerThread = maxQueueSizePerThread;
	m_threads.reserve(maxThreads);
}

ThreadPoolBase::ThreadPoolBase(int taskQueuePopTimeout)
{
	m_maxThreads = 0;
	m_maxQueueSizePerThread = 0;

	m_state = state_Initial;
	m_taskQueuePopTimeout = taskQueuePopTimeout;
}

ThreadPoolBase::~ThreadPoolBase()
{
	boost::mutex::scoped_lock lock(m_mutex);

	assert(	state_Initial == m_state ||
			state_Stopped == m_state );

	for (int i = 0; i != m_threads.size(); ++i)
	{
		delete m_threads[i];
	}
	m_threads.resize(0);
}

// 获取任务队列的尺寸最小，但又不为 0 的线程
WorkThreadBase* ThreadPoolBase::selectMinQueueThread(int* minSize) const
{
	*minSize  = m_maxQueueSizePerThread;
	WorkThreadBase* pThread = 0;
	for (int i = 0; i != m_threads.size(); ++i)
	{
		int size = m_threads[i]->taskQueueSize();

		// prefer to keep empty thread empty along,
		// except for other threads are all full.
		//
		// not return an empty thread,
		//    the reason is the empty threads are in 'freeThreads'.
		// when all threads are full,
		//   addTask() will wait for an available thread.
		// else
		//   keep empty thread empty, and addTask to other minSize thread
		//   this manner will minimize the thread switch overhead..
		if (0 != size && size < *minSize)
		{
			*minSize = size;
			pThread = m_threads[i];
		}
	}
	return pThread;
}

/*
WorkThreadBase* ThreadPoolBase::getAvailableThread()
{
	WorkThreadBase* pThread;
	{
		boost::mutex::scoped_lock lock(m_mutex);

		assert(	state_Initial == this->m_state ||
				state_Running == this->m_state );
		assert(0 != this->m_maxThreads);
		assert(0 != this->m_maxQueueSizePerThread);
		assert(m_threads.size() <= m_maxThreads);

		int minSize = m_maxQueueSizePerThread;
		pThread = selectMinQueueThread(&minSize);

		assert(minSize <= m_maxQueueSizePerThread);
		if (minSize == m_maxQueueSizePerThread)
		{	// all thread's taskQueue are full..
			// have not selected an available thread..
			if (m_threads.size() == m_maxThreads) {
				pThread = 0;
			} else {
				pThread = newThread();
				m_threads.push_back(pThread);
				pThread->start();
			}
		}
	}
	if (0 == pThread)
		pThread = m_freeThreads.remove_begin_elem(); // maybe waiting

	return pThread;
}
*/

WorkThreadBase* ThreadPoolBase::getAvailableThread()
{
	WorkThreadBase* pThread = 0;
	{
		boost::mutex::scoped_lock lock(m_mutex);

		assert(	state_Initial == this->m_state ||
				state_Running == this->m_state );
		assert(0 != this->m_maxThreads);
		assert(0 != this->m_maxQueueSizePerThread);
		assert((int)m_threads.size() <= m_maxThreads);

		if (m_threads.size() != m_maxThreads) {
			m_threads.push_back(pThread = newThread());
			pThread->start();
		}
	}
	if (0 == pThread)
		pThread = m_freeThreads.remove_begin_elem(); // maybe waiting

	return pThread;
}

// wait until all thread's taskQueue are empty
// then stop all threads ...
void ThreadPoolBase::complete()
{
	assert(	state_Initial == this->m_state ||
			state_Running == this->m_state );
	assert(0 != this->m_maxThreads);
	assert(0 != this->m_maxQueueSizePerThread);

	m_state = state_Stopping;

	while (true)
	{
		{
			boost::mutex::scoped_lock lock(m_mutex);
			int i;
			for (i = 0; i != m_threads.size(); ++i)
			{
				if (m_threads[i]->taskQueueSize() != 0)
					break;
			}
			if (m_threads.size() == i)
				break;
		}
		Thread::sleep(m_taskQueuePopTimeout);
	}

	// critical section...
	{
		boost::mutex::scoped_lock lock(m_mutex);

		for (int i = 0; i != m_threads.size(); ++i)
			m_threads[i]->stop();
	}
	for (int i = 0; i != m_threads.size(); ++i)
	{
		m_threads[i]->join();
	}
	m_state = state_Stopped;
}

void ThreadPoolBase::stop()
{
	// critical section...
	{
		boost::mutex::scoped_lock lock(m_mutex);

		assert(	state_Initial == this->m_state ||
				state_Running == this->m_state );
		assert(0 != this->m_maxThreads);
		assert(0 != this->m_maxQueueSizePerThread);

		for (int i = 0; i != m_threads.size(); ++i)
		{
			m_threads[i]->stop();
		}
		m_state = state_Stopping;
	}
	for (int i = 0; i != m_threads.size(); ++i)
	{
		m_threads[i]->join();
	}
	m_state = state_Stopped;
}

struct compile_template_struct
{
	struct MyTask
	{
		int t;
	};
	void proc(const int& k, MyTask& t)
	{

	}
};
static void compile_template()
{
	MemberFunKeyThreadPool<
		compile_template_struct,
		int,
		compile_template_struct::MyTask,
		&compile_template_struct::proc
	> pool;
	int key;
	compile_template_struct::MyTask task;
	pool.addTask(key, task);
}

} } // namespace terark::thread


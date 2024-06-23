#ifndef __terark_thread_FastPipe_h__
#define __terark_thread_FastPipe_h__

#include <boost/thread/condition.hpp>
#include <boost/thread.hpp>
#include <vector>

class FastBytePipe
{
	struct FuckBoostTimeDuration
	{
		int msec;
		FuckBoostTimeDuration(int x) : msec(x) {}
		int total_milliseconds() const { return msec;}
	};
	boost::mutex m_mutex;
	boost::condition m_cond;
	std::vector<char> m_buf;
	int head, tail;
public:
	int readsome(void* buf, int len, int timeout)
	{
		using namespace std;
		boost::mutex::scoped_lock lock(m_mutex);
		while (head == tail) {
			if (!m_cond.timed_wait(lock, FuckBoostTimeDuration(timeout)))
				return -1;
		}
		if (head < tail) {
			int n = min(len, tail - head);
			memcpy(buf, &m_buf[0] + head, n);
			head += n;
			return n;
		}
		else {
			assert(head > tail);
			int nn = m_buf.size();
			int n1 = nn - head;
			if (n1 <= len) {
				memcpy(buf, &m_buf[0] + head, n1);
				int n2 = min(len - n1, tail);
				memcpy(buf + n1, &m_buf[0], n2);
				head = n2;
				return n1 + n2;
			}
			else {
				memcpy(buf, &m_buf[0] + head, len);
				head += len;
				return len;
			}
		}
	}
	int writesome(void* buf, int len, int elem_size, int timeout)
	{
		using namespace std;
		boost::mutex::scoped_lock lock(m_mutex);
		while (head == tail) {
			if (!m_cond.timed_wait(lock, FuckBoostTimeDuration(timeout)))
				return -1;
		}
		if (tail < head) {
			int n = min(len, head - tail - elem_size);
			memcpy(&m_buf[0] + head, buf, n);
			tail += n;
			return n;
		}
		else { // tail >= head
			int nn = m_buf.size();
			assert(tail - head < nn);
			int availabe = (nn - tail) + head - elem_size;
			int n = min(availabe, len);
			if (0 == head) {
				memcpy(&m_buf[0] + tail, buf, n);
				tail += n;
				return n;
			}
			else {
				int n1 = nn - tail;
				if (n1 <= len) {
					memcpy(&m_buf[0] + tail, buf, n1);
					int n2 = n - n1;
					memcpy(&m_buf[0], buf + n1, n2);
					tail = n2;
					return n1 + n2;
				}
				else {
					memcpy(&m_buf[0] + tail, buf, len);
					tail += len;
					return len;
				}
			}
		}
	}
};

#endif // __terark_thread_FastPipe_h__

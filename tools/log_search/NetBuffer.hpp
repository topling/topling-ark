#ifndef __terark_NetBuffer_hpp__
#define __terark_NetBuffer_hpp__

#include <terark/fstring.hpp>
#include <terark/valvec.hpp>
#include <sys/socket.h>

namespace terark {

class NetBuffer {
protected:
	int  m_fd;
	valvec<char> m_buf;
	long m_head;
	long m_tail;
	long long m_lineCount;
	long long m_recvBytes;
public:
	NetBuffer(int fd, long capacity = 8*1024) {
		m_fd = fd;
		m_buf.resize_no_init(capacity);
		m_head = 0;
		m_tail = 0;
		m_lineCount = 0;
		m_recvBytes = 0;
	}
	virtual ~NetBuffer() {
		if (m_fd >= 0)
			close(m_fd);
	}

	int fd() const { return m_fd; }

	/**
	 * @returns errno
	 *   0      : EOF
	 *   EAGAIN :
	 *   Others : as errno
	 */
	int getNetLines(void* context) {
		assert(m_tail <= (long)m_buf.size());
		assert(m_tail >= m_head);
		assert(!m_buf.empty());
#if !defined(NDEBUG)
		fprintf(stderr,
			   	"NetBuffer: fd=%d lineCount=%lld recvBytes=%lld head=%ld tail=%ld\n",
			   	m_fd, m_lineCount, m_recvBytes, m_head, m_tail);
#endif
		int err = -1;
		while (-1 == err) {
			char* buf = m_buf.data();
			long searchPos = m_tail;
			long n1, n2;
			while ((n1 = long(m_buf.size()) - m_tail) > 0) {
				n2 = ::recv(m_fd, buf + m_tail, n1, MSG_DONTWAIT);
				if (n2 < 0) {
					err = errno; // EAGAIN or Others
					if (EAGAIN != err) {
						fprintf(stderr,
							"ERROR: recv(fd=%d, len=%ld, DONTWAIT) = %ld; recvBytes=%lld; errno=%d: %s\n",
							m_fd, n1, n2, m_recvBytes, err, strerror(err));
					}
					break;
				}
				if (0 == n2) {
					err = 0; // EOF
					break;
				}
				m_tail += n2;
				m_recvBytes += n2;
			}
			while (searchPos < m_tail) {
				char* lineEnd = (char*)memchr(buf + searchPos, '\n', m_tail - searchPos);
				if (NULL == lineEnd) {
					break;
				}
				lineEnd++;  // include '\n'
				char* lineBeg = buf + m_head;
				onOneLine(fstring(lineBeg, lineEnd), context);
			//	printf("%lld: %.*s\n", m_lineCount, int(lineEnd-lineBeg), lineBeg);
				m_lineCount++;
				m_head = searchPos = lineEnd - buf;
			}
			if (0 == err) { // EOF
				if (m_head < m_tail) {
					this->onOneLine(fstring(buf + m_head, buf + m_tail), context);
					this->m_lineCount++;
				}
				m_head = m_tail = 0;
			}
			else if (m_head == m_tail) {
				m_head = m_tail = 0;
			}
			else if (m_head) {
				long n = m_tail - m_head;
				memmove(buf, buf + m_head, n);
				m_head = 0;
				m_tail = n;
			}
			else if (long(m_buf.size()) == m_tail) {
				fprintf(stderr, "WARNING: double bufsize: old=%ld\n", m_tail);
				m_buf.resize_no_init(2 * m_tail);
			}
		}
		return err;
	}

	virtual void onOneLine(fstring line, void* context) = 0;

	long head() const { return m_head; }
	long tail() const { return m_tail; }
	long long recvBytes() const { return m_recvBytes; }
};

} // namespace terark

#endif // __terark_NetBuffer_hpp__


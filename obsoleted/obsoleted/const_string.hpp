/* vim: set tabstop=4 : */
#ifndef __terark_basic_const_string
#define __terark_basic_const_string

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#include <assert.h>
#include <stdexcept>
#include <iosfwd>
#include <algorithm>
#include <string>

#include <terark/io/var_int.hpp>

namespace terark {

	inline const wchar_t* NullString(const wchar_t* ) { return L""; }
	inline const char*    NullString(const char   * ) { return  ""; }

	/// \brief Wrapper for <tt>const CharT*</tt> to make it behave in a
	/// way more useful to MySQL++.
	///
	/// This class implements a small subset of the standard string class.
	///
	/// Objects are created from an existing <tt>const CharT*</tt> variable
	/// by copying the pointer only.  Therefore, the object pointed to by
	/// that pointer needs to exist for at least as long as the basic_const_string
	/// object that wraps it.
	template<class CharT>
	class basic_const_string
	{
	public:
		/// \brief Type of the data stored in this object, when it is not
		/// equal to SQL null.
		typedef const CharT value_type;

		/// \brief Type of "size" integers
		typedef unsigned int size_type;

		/// \brief Type used when returning a reference to a character in
		/// the string.
		typedef const CharT& const_reference;

		/// \brief Type of iterators
		typedef const CharT* const_iterator;

		/// \brief Same as const_iterator because the data cannot be
		/// changed.
		typedef const_iterator iterator;

		typedef std::pair<const CharT*, const CharT*> pair_t;

#if !defined(DOXYGEN_IGNORE)
		// Doxygen will not generate documentation for this section.
		typedef int difference_type;
		typedef const_reference reference;
		typedef const CharT* const_pointer;
		typedef const_pointer pointer;
#endif // !defined(DOXYGEN_IGNORE)

		/// \brief Create empty string
		basic_const_string()
		{
			m_end = m_beg = NullString((const CharT*)(0));
		}

		/// \brief Initialize string from existing std::string
		template<class CharTraits, class Alloc>
		basic_const_string(const std::basic_string<CharT, CharTraits, Alloc>& str)
		{
			m_beg = str.c_str();
			m_end = m_beg + str.size();
		}

		/// \brief Initialize string from existing C string
		explicit basic_const_string(const CharT* str)
		{
			m_beg = str;
#if defined(_DEBUG) || !defined(NDEBUG)
			int n = 64*1024;
			while (n && *str) ++str, --n;
			assert(0 != n);
#else
			while (*str) ++str;
#endif
			m_end = str;
		}

		/// \brief Initialize string from existing C string of known length
		basic_const_string(const CharT* str, size_type len)
		{
			m_beg = str;
			m_end = str + len;
		}

		/// \brief Initialize string from existing range of known range
		basic_const_string(const CharT* str_beg, const CharT* str_end)
		{
			assert(str_beg <= str_end);
			m_beg = str_beg;
			m_end = str_end;
		}

		/// \brief Initialize string from existing range of known range
		basic_const_string(const pair_t& range)
		{
			assert(range.first <= range.second);
			m_beg = range.first;
			m_end = range.second;
		}

		bool identical(const basic_const_string& y) const
		{
			return m_beg == y.m_beg && m_end == y.m_end;
		}

		pair_t to_pair() const { return pair_t(m_beg, m_end); }

		template<class StdString>
		StdString to_str() const
		{
			return StdString(m_beg, m_end);
		}
		std::basic_string<CharT> to_str() const
		{
			return std::basic_string<CharT>(m_beg, m_end);
		}

		/// \brief Assignment operator
		basic_const_string& operator=(const CharT* str)
		{
			m_beg = str;
			m_end = str + strlen(str);
			return *this;
		}

		/// \brief Return number of characters in string
		size_type length() const { return m_end - m_beg; }

		/// \brief Return number of characters in string
		size_type size() const { return m_end - m_beg; }

		bool empty() const { return m_beg == m_end; }

		/// \brief Return iterator pointing to the first character of
		/// the string
		const_iterator begin() const { return m_beg; }

		/// \brief Return iterator pointing to one past the last character
		/// of the string.
		const_iterator end() const { return m_end; }

		/// \brief Return the maximum number of characters in the string.
		///
		/// Because this is a \c const string, this is just an alias for
		/// size(); its size is always equal to the amount of data currently
		/// stored.
		size_type max_size() const { return size(); }

		/// \brief Return a reference to a character within the string.
		const_reference operator [](int pos) const
		{
			assert(pos >= 0);
			assert(pos <= int(size()));
			return m_beg[pos];
		}

		/// \brief Return a reference to a character within the string.
		///
		/// Unlike \c operator[](), this function throws an
		/// \c std::out_of_range exception if the index isn't within range.
		const_reference at(size_type pos) const
		{
			if (pos >= size())
				throw std::out_of_range(BOOST_CURRENT_FUNCTION);
			else
				return m_beg[pos];
		}

		/// \brief Return a const pointer to the string data.  Not
		/// necessarily null-terminated!
//		const CharT* c_str() const { return m_beg; }

		/// \brief return string data
		const CharT* data() const { return m_beg; }

		basic_const_string substr(int start) const
		{
			using namespace std;
			assert(start >= 0);
			assert(start <= size());
			return basic_const_string(m_beg, m_beg + start);
		}
		basic_const_string substr(int start, int finish) const
		{
			assert(start >= 0);
			assert(start <= finish);
			assert(start <= int(size()));
			assert(finish <= int(size()));
			return basic_const_string(m_beg + start, m_beg + finish);
		}

		basic_const_string suffix(int start) const
		{
			assert(start >= 0);
			assert(start <= int(size()));
			return basic_const_string(m_beg + start, m_end);
		}

		/// \brief Lexically compare this string to another.
		///
		/// \param str string to compare against this one
		///
		/// \retval <0 if str1 is lexically "less than" str2
		/// \retval 0 if str1 is equal to str2
		/// \retval >0 if str1 is lexically "greater than" str2
		int compare(const basic_const_string& str) const
		{
			using namespace std;
			const CharT* p1 = m_beg;
			const CharT* p2 = str.m_beg;
			for (size_type cnt = min(length(), str.length()); cnt ; --cnt, ++p1, ++p2)
			{
				if (*p1 != *p2)
					return *p1 - *p2;
			}
			return length() - str.length();
		}

		int compare(int _Off,
					const basic_const_string& _Right,
					int _Roff, int _Count) const
		{
			using namespace std;
			assert(_Off >= 0);
			assert(_Off <= this->length());
			assert(_Roff >= 0);
			assert(_Roff <= _Right.length());
			assert(_Count >= 0);
			return compare(basic_const_string(m_beg + _Off, min(m_beg + _Off + _Count, m_end)),
						   basic_const_string(_Right.m_beg + _Roff, min(_Right.m_beg + _Roff + _Count, _Right.m_end)));
		}

		size_type prefix_length(const basic_const_string& str) const
		{
			using namespace std;
			const CharT* p1 = m_beg;
			const CharT* p2 = str.m_beg;
			size_type min_len = min(length(), str.length());
			size_type cnt = min_len;
			for (; cnt ; --cnt, ++p1, ++p2)
			{
				if (*p1 != *p2)
					break;
			}
			return min_len - cnt;
		}

	private:
		const CharT* m_beg;
		const CharT* m_end;

//////////////////////////////////////////////////////////////////////////

	/// \brief Inserts a basic_const_string into a C++ stream
	friend
	inline std::ostream& operator<<(std::ostream& o, const basic_const_string<CharT>& str)
	{
		return o.write(str.data(), str.size());
	}

	/// \brief Calls lhs.compare(), passing rhs
	friend
	inline int compare(const basic_const_string<CharT>& lhs, const basic_const_string<CharT>& rhs)
	{
		return lhs.compare(rhs);
	}

	/// \brief Returns true if lhs is the same as rhs
	friend
	inline bool operator ==(const basic_const_string<CharT>& lhs, const basic_const_string<CharT>& rhs)
	{
		return lhs.compare(rhs) == 0;
	}

	/// \brief Returns true if lhs is not the same as rhs
	friend
	inline bool operator !=(const basic_const_string<CharT>& lhs, const basic_const_string<CharT>& rhs)
	{
		return lhs.compare(rhs) != 0;
	}

	/// \brief Returns true if lhs is lexically less than rhs
	friend
	inline bool operator <(const basic_const_string<CharT>& lhs, const basic_const_string<CharT>& rhs)
	{
		return lhs.compare(rhs) < 0;
	}

	/// \brief Returns true if lhs is lexically less or equal to rhs
	friend
	inline bool operator <=(const basic_const_string<CharT>& lhs, const basic_const_string<CharT>& rhs)
	{
		return lhs.compare(rhs) <= 0;
	}

	/// \brief Returns true if lhs is lexically greater than rhs
	friend
	inline bool operator >(const basic_const_string<CharT>& lhs, const basic_const_string<CharT>& rhs)
	{
		return lhs.compare(rhs) > 0;
	}

	/// \brief Returns true if lhs is lexically greater than or equal to rhs
	friend
	inline bool operator >=(const basic_const_string<CharT>& lhs, const basic_const_string<CharT>& rhs)
	{
		return lhs.compare(rhs) >= 0;
	}

	};

	/// DataIO storing
	template<class DataIO, class CharT>
	void DataIO_saveObject(DataIO& dio, const basic_const_string<CharT>& x)
	{
		var_size_t size(x.size());
		dio << size;
		dio.ensureWrite(x.data(), size);
	}
	template<class DataIO, class CharT>
	void DataIO_loadObject(DataIO& dio, basic_const_string<CharT>& x)
	{
		dio.dont_support_loading(x);
	}

//////////////////////////////////////////////////////////////////////////

	typedef basic_const_string<char> const_string;
	typedef basic_const_string<wchar_t> const_wstring;

} // namespace terark

#endif // __terark_basic_const_string


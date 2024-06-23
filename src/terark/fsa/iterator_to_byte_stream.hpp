#pragma once
#include <utility>
#include <iterator>

namespace terark {

template<class InputIter>
class iterator_to_byte_stream : public std::pair<InputIter, InputIter> {
	iterator_to_byte_stream& operator++(int); // disable suffix operator++

public:
	iterator_to_byte_stream(InputIter beg, InputIter end)
	   	: std::pair<InputIter, InputIter>(beg, end) {}

	bool empty() const { return this->first == this->second; }

	typename std::iterator_traits<InputIter>::value_type
	operator*() const {
		assert(this->first != this->second);
		return *this->first;
	}

	iterator_to_byte_stream& operator++() {
		assert(this->first != this->second);
		++this->first;
		return *this;
	}

	typedef InputIter iterator, const_iterator;
	InputIter begin() const { return this->first ; }
	InputIter   end() const { return this->second; }
};

} // namespace terark

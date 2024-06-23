#ifndef __terark_automata_multi_regex_hpp__
#define __terark_automata_multi_regex_hpp__

#include <re2/filtered_re2.h>
#include <boost/noncopyable.hpp>

class MultiRegex : private boost::noncopyable {
	class Impl;
	Impl* impl;

public:
	MultiRegex();
	~MultiRegex();

	re2::RE2::ErrorCode
	add(re2::StringPiece pattern, const RE2::Options& options, int *id);

	void compile();

	void scan(re2::StringPiece text, std::vector<int>* matched_regexps);

	const re2::RE2* GetRE2(int id) const;
};



#endif // __terark_automata_multi_regex_hpp__


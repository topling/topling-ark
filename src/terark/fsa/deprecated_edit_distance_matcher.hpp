#pragma once

#include <terark/fstring.hpp>
#include <terark/valvec.hpp>
#include <terark/hash_strmap.hpp>
#include <terark/util/fstrvec.hpp>

namespace terark {

class TERARK_DLL_EXPORT EditDistanceMatcher : boost::noncopyable
  , public  valvec<hash_strmap<size_t> > {
	typedef valvec<hash_strmap<size_t> > super;
public:
	struct Elem {
		size_t  state;
		size_t  edit_distance;
	};
	valvec<Elem> q1, q2;
	fstrvec      s1, s2;
	size_t       n_try;

	EditDistanceMatcher(size_t max_edit_distance1);
	EditDistanceMatcher();
	~EditDistanceMatcher();
	void start();
	void swap12();
	void set_max_edit_distance(size_t ed) { this->resize(ed + 1); }
	size_t max_edit_distance() const { return this->size() - 1; }
};

} // namespace terark

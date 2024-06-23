#include "deprecated_edit_distance_matcher.hpp"

namespace terark {

EditDistanceMatcher::EditDistanceMatcher(size_t ed) : super(ed+1), n_try(0) {}
EditDistanceMatcher::EditDistanceMatcher() : n_try(0) {}
EditDistanceMatcher::~EditDistanceMatcher() {}

void EditDistanceMatcher::start() {
	n_try = 0;
	q1.erase_all();
	q2.erase_all();
	s1.erase_all();
	s2.erase_all();
	for (size_t i = 0; i < this->size(); ++i) (*this)[i].erase_all();
}
void EditDistanceMatcher::swap12() {
	q1.swap(q2);
	s1.swap(s2);
}

} // namespace terark


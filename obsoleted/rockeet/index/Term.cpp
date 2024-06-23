#include "Term.hpp"
#include <stdexcept>

namespace terark { namespace rockeet {

	ITerm::~ITerm()
	{

	}

	//////////////////////////////////////////////////////////////////////////

	void TermVec::push_back(ITerm* term)
	{
		if (!this->empty() && typeid(*this->front()) != typeid(*term))
		{
			throw std::runtime_error("terms on same field must has same type");
		}
		std::vector<ITerm*>::push_back(term);
	}

	void TermVec::clear()
	{
		for (TermVec::iterator j = begin(); j != end(); ++j)
			(*j)->destroy();
		std::vector<ITerm*>::clear();
	}

//////////////////////////////////////////////////////////////////////////
	DocTermVec::DocTermVec()
	{

	}

	DocTermVec::~DocTermVec()
	{
		for (iterator i = begin(); i != end();)
			this->erase(i++);
	}

	void DocTermVec::erase(iterator iter)
	{
		iter->second.clear();
		super::erase(iter);
	}


} } // namespace terark::rockeet


namespace terark {
template<class State>
class MyDAWG : public DAWG<State> {
	typedef DAWG<State> super;
	typedef MyDAWG MyType;
public:
#include <terark/fsa/ppi/dfa_using_template_base.hpp>
#include <terark/fsa/ppi/accept.hpp>
#include <terark/fsa/ppi/adfa_minimize.hpp>
#include <terark/fsa/ppi/dfa_hopcroft.hpp>
};

} // namespace terark

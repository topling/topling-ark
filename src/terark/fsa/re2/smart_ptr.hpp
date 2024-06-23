#ifndef __terark_automata_re2_intrusive_ptr_hpp__
#define __terark_automata_re2_intrusive_ptr_hpp__

// for use with boost::intrusive_ptr
#include <re2/regexp.h>

namespace re2 {
	void intrusive_ptr_add_ref(Regexp* p) { p->Incref(); }
	void intrusive_ptr_release(Regexp* p) { p->Decref(); }
}

#endif // __terark_automata_re2_intrusive_ptr_hpp__


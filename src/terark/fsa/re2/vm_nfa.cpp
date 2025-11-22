#include "vm_nfa.hpp"
#include <terark/fsa/mre_delim.hpp>
#include <terark/bitmap.hpp>
#include <terark/util/throw.hpp>
#include <re2/prog.h> // virtual machine nfa of re2

namespace terark {

using re2::StringPiece;
using re2::Regexp;
using re2::Prog;

RE2_VM_NFA::RE2_VM_NFA(Prog* prog) {
	m_prog = prog;
	m_add_dot_star = false;
	m_exclude_char = 256;
	m_use_bin_val  = false;
	m_do_capture  = false;
}

RE2_VM_NFA::~RE2_VM_NFA() {
}

// expand bytes to auchar_t
static void
back_append_bytes(basic_fstrvec<auchar_t>& sv, const void* data, size_t len) {
	const byte_t* bytes = reinterpret_cast<const byte_t*>(data);
	const size_t  oldsize = sv.strpool.size();
	sv.strpool.resize_no_init(oldsize + len);
	auchar_t* dst = sv.strpool.data() + oldsize;
	for (size_t i = 0; i < len; ++i) dst[i] = bytes[i];
	sv.offsets.back() += len;
}
static void
back_append_str(basic_fstrvec<auchar_t>& sv, fstring str) {
	back_append_bytes(sv, str.p, str.n);
}

void RE2_VM_NFA::compile() {
	assert(NULL != m_prog);
	assert(-1 != m_exclude_char);
	if (-1 == m_exclude_char) {
		THROW_STD(invalid_argument
			, "shouldn't be called when m_exclude_char=%d(0x%X)"
			, m_exclude_char, m_exclude_char
			);
	}
	m_cap_ptrs.resize(m_prog->size(), -1);
	febitvec color(m_prog->size(), 0);
	valvec<int>  stack;
	int start = m_add_dot_star ? m_prog->start_unanchored() : m_prog->start();
	color.set1(start);
	stack.push_back(start);
#define NFA_PUT_CHILD(x) \
do { \
	int child = x; \
	if (color.is0(child)) { \
		color.set1(child);  \
		stack.push_back(child); \
	} \
} while (0)
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	while (!stack.empty()) {
		const int parent = stack.back(); stack.pop_back();
		assert(parent < m_prog->size());
		Prog::Inst* inst = m_prog->inst(parent);
	//	fprintf(stderr, "%s: s=%d opcode=%d\n", BOOST_CURRENT_FUNCTION, parent, inst->opcode());
		switch (inst->opcode()) {
		default:
			assert(0);
			break;
		case re2::kInstByteRange:
			NFA_PUT_CHILD(inst->out());
			break;
		case re2::kInstMatch:
		  if (-1 != m_exclude_char) {
			auchar_t real_full_match_delim =
				m_exclude_char < 256 ?
				m_exclude_char + 0   :
				m_exclude_char + 1   ; //FULL_MATCH_DELIM;
			m_cap_ptrs[parent] = m_captures.size();
			m_captures.push_back();
			m_captures.back_append(real_full_match_delim);
			back_append_str(m_captures, m_val_prefix);
			m_captures.back_append('\0');
		  }
			break;
		case re2::kInstFail:
			break;
		case re2::kInstAlt:
		case re2::kInstAltMatch:
			NFA_PUT_CHILD(inst->out());
			NFA_PUT_CHILD(inst->out1());
			break;
		case re2::kInstCapture:
		  if (m_do_capture && -1 != m_exclude_char) {
			m_cap_ptrs[parent] = m_captures.size();
			m_captures.push_back();
			m_captures.back_append(m_exclude_char);
			if (m_use_bin_val) {
				assert(m_val_prefix.size() == 4);
				int32_t i32_cap_id = inst->cap();
				back_append_bytes(m_captures, &i32_cap_id, 4);
			} else {
				char buf[32];
				int  cap_id = inst->cap();
				int  len = sprintf(buf, "%X", cap_id);
				m_captures.back_append('\t');
				back_append_bytes(m_captures, buf, len);
			}
			m_captures.back_append('\0');
		  }
			// fall through
			no_break_fallthrough;
		case re2::kInstNop:
		case re2::kInstEmptyWidth:
			NFA_PUT_CHILD(inst->out());
			break;
		}
	}
	m_captures.shrink_to_fit();
}

size_t RE2_VM_NFA::total_states() const {
	// 0 == initial_state is reserved for terark nfa
	return m_prog->size() + 1 + m_captures.strpool.size();
}

bool RE2_VM_NFA::is_final(size_t s) const {
	assert(s < this->total_states());
	if (initial_state == s) {
		return false;
	}
	if (s <= (size_t)m_prog->size()) {
		if (-1 != m_exclude_char)
			return false;
		else
			return re2::kInstMatch == m_prog->inst(s-1)->opcode();
	}
	if (-1 != m_exclude_char) {
		const unsigned  j = s - (m_prog->size()+1);
		const unsigned* beg = m_captures.offsets.begin();
		const unsigned* end = m_captures.offsets.end();
		const unsigned* pos = std::upper_bound(beg, end, j);
		assert(j < m_captures.strpool.size());
		return j+1 == *pos;
	}
	assert(0);
	abort();
	return false;
}

void
RE2_VM_NFA::get_epsilon_targets(state_id_t s, valvec<state_id_t>* children)
const {
	assert(s < this->total_states());
	children->erase_all();
	if (initial_state == s) {
		if (m_add_dot_star)
			children->push_back(m_prog->start_unanchored()+1);
		else
			children->push_back(m_prog->start()+1);
		return;
	}
	if (s > m_prog->size()) {
		return;
	}
	Prog::Inst* inst = m_prog->inst(s-1);
	switch (inst->opcode()) {
	default:
		assert(0);
		break;
	case re2::kInstMatch:
		if (-1 != m_exclude_char) {
			const int pos = m_cap_ptrs[s-1];
			assert(-1 != pos);
			assert(pos < (int)m_captures.size());
			const unsigned child0 = m_captures.offsets[pos];
			children->push_back(m_prog->size()+1 + child0);
		}
		break;
	case re2::kInstByteRange:
	case re2::kInstFail:
	//	fprintf(stderr, "%s: s=%d opcode=%d\n", BOOST_CURRENT_FUNCTION, s, inst->opcode());
		break;
	case re2::kInstAlt:
	case re2::kInstAltMatch:
		children->push_back(inst->out()+1);
		children->push_back(inst->out1()+1);
		break;
	case re2::kInstCapture:
		if (m_do_capture && -1 != m_exclude_char) {
			const int pos = m_cap_ptrs[s-1];
			assert(-1 != pos);
			assert(pos < (int)m_captures.size());
			const unsigned child0 = m_captures.offsets[pos];
			children->push_back(m_prog->size()+1 + child0);
		}
		// fall through
		no_break_fallthrough;
	case re2::kInstNop:
	case re2::kInstEmptyWidth:
		children->push_back(inst->out()+1);
		break;
	}
}

void RE2_VM_NFA::get_non_epsilon_targets(state_id_t s,
			valvec<CharTarget<size_t> >* children)
const {
	assert(s < this->total_states());
	children->erase_all();
	if (initial_state == s) {
		return;
	}
	if (s > m_prog->size()) {
	    assert(-1 != m_exclude_char);
		// this is a state in cap_id string
		const unsigned  j = s - (m_prog->size()+1);
		const unsigned* beg = m_captures.offsets.begin();
		const unsigned* end = m_captures.offsets.end();
		const unsigned* pos = std::upper_bound(beg, end, j);
	   	if (j < *pos-1) {
			const auchar_t c = m_captures.strpool[j];
			children->emplace_back(c, s+1);
		}
		return;
	}
	Prog::Inst* inst = m_prog->inst(s-1);
	if (inst->opcode() == re2::kInstByteRange) {
		int lo = inst->lo();
		int hi = inst->hi();
		assert(lo >= 0);
		assert(lo <= hi);
		assert(hi - lo <= 255);
	/* // DO NOT uncomment the code, re2 has handled all case folding
		if (inst->foldcase()) {
			lo = tolower(lo);
			hi = tolower(hi);
		}
	*/
		for (int c = lo; c <= hi; ++c)
			if (c != m_exclude_char)
			{
				if (inst->foldcase() && c >= 'a' && c <= 'z') {
					children->emplace_back(c - ('a' - 'A'), inst->out()+1);
				}
				children->emplace_back(c, inst->out()+1);
			}
	} else if (inst->opcode() == re2::kInstEmptyWidth) {
		const re2::EmptyOp eop = inst->empty();
		if (re2::kEmptyWordBoundary == eop) {
			children->emplace_back(REGEX_DFA_WORD_BOUNDARY, inst->out()+1);
		} else if (re2::kEmptyNonWordBoundary == eop) {
			children->emplace_back(REGEX_DFA_NON_WORD_BOUNDARY, inst->out()+1);
		} else {
			// do nothing
		}
	} else {
	//	fprintf(stderr, "%s: s=%d opcode=%d\n", BOOST_CURRENT_FUNCTION, s, inst->opcode());
	}
}

} // namespace terark


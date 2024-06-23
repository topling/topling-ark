#include "mre_match.hpp"
#include "fsa.hpp"
#include <terark/lcast.hpp>
#include <terark/util/linebuf.hpp>
#include <terark/util/autoclose.hpp>
#include <terark/util/throw.hpp>
#include <terark/util/unicode_iterator.hpp>

namespace terark {

MultiRegexMatchOptions::~MultiRegexMatchOptions() {
	delete m_dfa;
}

MultiRegexMatchOptions::MultiRegexMatchOptions() {
	enableDynamicDFA = true;
	m_dfa = NULL;
}
MultiRegexMatchOptions::MultiRegexMatchOptions(fstring _dfaFilePath) {
	enableDynamicDFA = true;
	m_dfa = NULL;
	load_dfa(_dfaFilePath);
}

void MultiRegexMatchOptions::load_dfa() {
	load_dfa(dfaFilePath);
}
void MultiRegexMatchOptions::load_dfa(fstring _dfaFilePath) {
	if (_dfaFilePath.empty()) {
		THROW_STD(invalid_argument, "dfaFilePath is empty");
	}
	dfaFilePath.assign(_dfaFilePath.data(), _dfaFilePath.size());
	delete m_dfa;
	m_dfa = BaseDFA::load_from(dfaFilePath);
}

MultiRegexMatchOptions::MultiRegexMatchOptions(const MultiRegexMatchOptions&)
= default;

MultiRegexMatchOptions&
MultiRegexMatchOptions::operator=(const MultiRegexMatchOptions&)
= default;

///////////////////////////////////////////////////////////////////////////////

#define IMPLEMENT_find_first(StepBytes, MatchCall) \
	const char* pos = text.begin(); \
	const char* end = text.end(); \
	while (pos < end) { \
		fstring suffix(pos, end); \
		size_t len = MatchCall; \
		if (len) { \
			assert(!this->empty()); \
			return PosLen(int(pos - text.p), int(len)); \
		} \
		else { \
			pos += StepBytes; \
		} \
	} \
	return PosLen(text.ilen(), 0)

#define IMPLEMENT_find_all(StepBytes, MatchCall) \
	m_all_match.erase_all(); \
	const char* pos = text.begin(); \
	const char* end = text.end(); \
	while (pos < end) { \
		fstring suffix(pos, end); \
		size_t len = MatchCall; \
		if (len) { \
			assert(!this->empty()); \
			for (size_t i = 0; i < this->size(); ++i) { \
                int ipos = int(pos-text.p); \
                int regex_id = m_regex_idvec[i]; \
				m_all_match.emplace_back(ipos, int(len), regex_id); \
			} \
		} \
		pos += StepBytes; \
	} \
	return m_all_match.size()

MultiRegexFullMatch::PosLen
MultiRegexFullMatch::utf8_find_first(fstring text) {
	IMPLEMENT_find_first(utf8_byte_count(*pos), match(suffix));
}

MultiRegexFullMatch::PosLen
MultiRegexFullMatch::utf8_find_first(fstring text, const ByteTR& tr) {
	IMPLEMENT_find_first(utf8_byte_count(*pos), match(suffix, tr));
}

MultiRegexFullMatch::PosLen
MultiRegexFullMatch::utf8_find_first(fstring text, const byte_t* tr) {
	IMPLEMENT_find_first(utf8_byte_count(*pos), match(suffix, tr));
}

MultiRegexFullMatch::PosLen
MultiRegexFullMatch::shortest_utf8_find_first(fstring text) {
	IMPLEMENT_find_first(utf8_byte_count(*pos), shortest_match(suffix));
}

MultiRegexFullMatch::PosLen
MultiRegexFullMatch::shortest_utf8_find_first(fstring text, const ByteTR& tr) {
	IMPLEMENT_find_first(utf8_byte_count(*pos), shortest_match(suffix, tr));
}

MultiRegexFullMatch::PosLen
MultiRegexFullMatch::shortest_utf8_find_first(fstring text, const byte_t* tr) {
	IMPLEMENT_find_first(utf8_byte_count(*pos), shortest_match(suffix, tr));
}

MultiRegexFullMatch::PosLen
MultiRegexFullMatch::byte_find_first(fstring text) {
	IMPLEMENT_find_first(1, match(suffix));
}

MultiRegexFullMatch::PosLen
MultiRegexFullMatch::byte_find_first(fstring text, const ByteTR& tr) {
	IMPLEMENT_find_first(1, match(suffix, tr));
}

MultiRegexFullMatch::PosLen
MultiRegexFullMatch::byte_find_first(fstring text, const byte_t* tr) {
	IMPLEMENT_find_first(1, match(suffix, tr));
}

MultiRegexFullMatch::PosLen
MultiRegexFullMatch::shortest_byte_find_first(fstring text) {
	IMPLEMENT_find_first(1, shortest_match(suffix));
}

MultiRegexFullMatch::PosLen
MultiRegexFullMatch::shortest_byte_find_first(fstring text, const ByteTR& tr) {
	IMPLEMENT_find_first(1, shortest_match(suffix, tr));
}

MultiRegexFullMatch::PosLen
MultiRegexFullMatch::shortest_byte_find_first(fstring text, const byte_t* tr) {
	IMPLEMENT_find_first(1, shortest_match(suffix, tr));
}

//---------------------------------------------------------------------------

size_t
MultiRegexFullMatch::utf8_find_all(fstring text) {
	IMPLEMENT_find_all(utf8_byte_count(*pos), match(suffix));
}

size_t
MultiRegexFullMatch::utf8_find_all(fstring text, const ByteTR& tr) {
	IMPLEMENT_find_all(utf8_byte_count(*pos), match(suffix, tr));
}

size_t
MultiRegexFullMatch::utf8_find_all(fstring text, const byte_t* tr) {
	IMPLEMENT_find_all(utf8_byte_count(*pos), match(suffix, tr));
}

size_t
MultiRegexFullMatch::shortest_utf8_find_all(fstring text) {
	IMPLEMENT_find_all(utf8_byte_count(*pos), shortest_match(suffix));
}

size_t
MultiRegexFullMatch::shortest_utf8_find_all(fstring text, const ByteTR& tr) {
	IMPLEMENT_find_all(utf8_byte_count(*pos), shortest_match(suffix, tr));
}

size_t
MultiRegexFullMatch::shortest_utf8_find_all(fstring text, const byte_t* tr) {
	IMPLEMENT_find_all(utf8_byte_count(*pos), shortest_match(suffix, tr));
}

size_t
MultiRegexFullMatch::byte_find_all(fstring text) {
	IMPLEMENT_find_all(1, match(suffix));
}

size_t
MultiRegexFullMatch::byte_find_all(fstring text, const ByteTR& tr) {
	IMPLEMENT_find_all(1, match(suffix, tr));
}

size_t
MultiRegexFullMatch::byte_find_all(fstring text, const byte_t* tr) {
	IMPLEMENT_find_all(1, match(suffix, tr));
}

size_t
MultiRegexFullMatch::shortest_byte_find_all(fstring text) {
	IMPLEMENT_find_all(1, shortest_match(suffix));
}

size_t
MultiRegexFullMatch::shortest_byte_find_all(fstring text, const ByteTR& tr) {
	IMPLEMENT_find_all(1, shortest_match(suffix, tr));
}

size_t
MultiRegexFullMatch::shortest_byte_find_all(fstring text, const byte_t* tr) {
	IMPLEMENT_find_all(1, shortest_match(suffix, tr));
}

//@{
/// utf8_find_all_len
/// byte_find_all_len
/// return match position in m_all_match, PosLenRegexID::len is not the
///      exact matched len, it is just the suffix len: the max possible
///      len of each matched regex_id.
#define IMPLEMENT_find_all_len(StepBytes, MatchCall) \
	m_all_match.erase_all(); \
	const char* pos = text.begin(); \
	const char* end = text.end(); \
	while (pos < end) { \
		fstring suffix(pos, end); \
		size_t num = MatchCall; \
		if (num) { \
			assert(!this->empty()); \
			for (size_t i = 0; i < this->size(); ++i) { \
                int ipos = int(pos-text.p); \
                int ilen = int(end-pos); \
                int regex_id = m_regex_idvec[i]; \
				m_all_match.emplace_back(ipos, ilen, regex_id); \
			} \
		} \
		pos += StepBytes; \
	} \
	return m_all_match.size()

size_t MultiRegexFullMatch::utf8_find_all_len(fstring text) {
	IMPLEMENT_find_all_len(utf8_byte_count(*pos), match_all(suffix));
}
size_t MultiRegexFullMatch::utf8_find_all_len(fstring text, const ByteTR& tr) {
	IMPLEMENT_find_all_len(utf8_byte_count(*pos), match_all(suffix, tr));
}
size_t MultiRegexFullMatch::utf8_find_all_len(fstring text, const byte_t* tr) {
	IMPLEMENT_find_all_len(utf8_byte_count(*pos), match_all(suffix, tr));
}
size_t MultiRegexFullMatch::byte_find_all_len(fstring text) {
	IMPLEMENT_find_all_len(1, match_all(suffix));
}
size_t MultiRegexFullMatch::byte_find_all_len(fstring text, const ByteTR& tr) {
	IMPLEMENT_find_all_len(1, match_all(suffix, tr));
}
size_t MultiRegexFullMatch::byte_find_all_len(fstring text, const byte_t* tr) {
	IMPLEMENT_find_all_len(1, match_all(suffix, tr));
}
//@}

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

MultiRegexSubmatch::MultiRegexSubmatch() {
	dfa = NULL;
	m_first_pass = NULL;
}

MultiRegexSubmatch::MultiRegexSubmatch(const MultiRegexSubmatch& y)
  : dfa(y.dfa)
  , m_fullmatch_regex(y.m_fullmatch_regex)
  , cap_pos_data(y.cap_pos_data)
  , cap_pos_ptr(y.cap_pos_ptr)
  , m_first_pass(NULL)
{
	if (y.m_first_pass)
		m_first_pass = y.m_first_pass->clone();
}

MultiRegexSubmatch::~MultiRegexSubmatch() {
	delete m_first_pass;
}

inline bool is_utf8_tail(unsigned char c) {
	return (c & 0xC0) == 0x80;
}

///@note text must be the text used in previous match(...)
void MultiRegexSubmatch::fix_utf8_boundary(fstring text) {
	for(int regex_id : m_fullmatch_regex) {
		int* cap_beg = cap_pos_ptr[regex_id+0];
		int* cap_end = cap_pos_ptr[regex_id+1];
		assert(cap_end - cap_beg >= 2);
		// cap[0,1] is the fullmatch, needn't to trace back to utf8 boundary
		// just for submatches:
		for (int* pi = cap_beg + 2; pi < cap_end; ++pi) {
			ptrdiff_t i = *pi;
			if (i == text.n)
			   	break;
			while (i > 0 && is_utf8_tail(text.p[i])) --i;
			*pi = (int)i;
		}
	}
}

void MultiRegexSubmatch::reset() {
	m_fullmatch_regex.erase_all();
	cap_pos_data.fill(-1); // indicate all are unmatched
	ptrdiff_t n = cap_pos_ptr.size() - 1;
	for (ptrdiff_t i = 0; i < n; ++i)
		 *cap_pos_ptr[i] = 0; // the full match start position
}

void MultiRegexSubmatch::reset(int regex_id) {
	assert(regex_id < (int)cap_pos_ptr.size() - 1);
	int* beg = cap_pos_ptr[regex_id+0];
	int* end = cap_pos_ptr[regex_id+1];
	beg[0] = 0; // the full match start position
	for (int* p = beg+1; p < end; ++p) *p = -1;
}

size_t MultiRegexSubmatch::match_utf8(fstring text) {
	size_t ret = match(text);
	fix_utf8_boundary(text);
	return ret;
}
size_t MultiRegexSubmatch::match_utf8(fstring text, const ByteTR& tr) {
	size_t ret = match(text, tr);
	fix_utf8_boundary(text);
	return ret;
}
size_t MultiRegexSubmatch::match_utf8(fstring text, const byte_t* tr) {
	size_t ret = match(text, tr);
	fix_utf8_boundary(text);
	return ret;
}

void MultiRegexSubmatch::push_regex_info(int n_submatch) {
	int* oldptr  = cap_pos_data.data();
	int  oldsize = cap_pos_data.size();
	cap_pos_ptr.push_back(oldptr + oldsize);
	cap_pos_data.resize(cap_pos_data.size() + n_submatch*2, -1);
	int* newptr = cap_pos_data.data();
	if (newptr != oldptr) {
		for (size_t i = 0; i < cap_pos_ptr.size(); ++i)
			cap_pos_ptr[i] = newptr + (cap_pos_ptr[i] - oldptr);
	}
	newptr[oldsize] = 0; // the full match start position
}

void MultiRegexSubmatch::complete_regex_info() {
	cap_pos_ptr.push_back(cap_pos_data.end());
}

std::pair<int,int>
MultiRegexSubmatch::get_match_range(int regex_id, int sub_id)
const {
	assert(regex_id >= 0);
	assert(regex_id < (int)cap_pos_data.size());
    const int* pcap = cap_pos_ptr[regex_id];
    assert(pcap + 2*sub_id+1 < cap_pos_ptr[regex_id + 1]);
	return std::pair<int,int>(pcap[2*sub_id], pcap[2*sub_id+1]);
}

fstring
MultiRegexSubmatch::operator()(const char* base, int regex_id, int sub_id)
const {
	assert(regex_id >= 0);
	assert(regex_id < (int)cap_pos_data.size());
	assert(sub_id*2 < cap_pos_ptr[regex_id+1] - cap_pos_ptr[regex_id]);
    const int* pcap = cap_pos_ptr[regex_id];
	int beg = pcap[2*sub_id+0];
	int end = pcap[2*sub_id+1];
	if (-1 == beg || -1 == end)
		return "";
	if (end <= beg)
		return "";
    assert(pcap + 2*sub_id+1 < cap_pos_ptr[regex_id + 1]);
	return fstring(base + beg, end - beg);
}

/////////////////////////////////////////////////////////////////////////

} // namespace terark


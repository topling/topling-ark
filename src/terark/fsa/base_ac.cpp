#include "base_ac.hpp"
#include "x_fsa_util.hpp"
#include "dfa_mmap_header.hpp"

namespace terark {

BaseAC::BaseAC(const BaseAC&) = default;
BaseAC& BaseAC::operator=(const BaseAC& y) = default;

BaseAC::BaseAC() {
	n_words = 0;
}
BaseAC::~BaseAC() {
}

std::string
BaseAC::restore_word(size_t stateID, size_t wID)
const {
	std::string word;
	restore_word(stateID, wID, &word);
	return word;
}
void
BaseAC::restore_word(size_t stateID, size_t wID, std::string* word)
const {
	THROW_STD(invalid_argument,
		"Method is not implmented, try to use DoubleArrayTrie AC Automata");
}

void BaseAC::ac_str_finish_load_mmap(const DFA_MmapHeader* base) {
	byte_t* bbase = (byte_t*)base;
	auto   ac_str_blocks = &base->blocks[base->num_blocks - base->ac_word_ext];
	n_words = size_t(base->dawg_num_words);
	offsets.clear();
	strpool.clear();
	if (base->ac_word_ext >= 1) {
		size_t offsize = size_t(ac_str_blocks[0].length / sizeof(offset_t));
		assert(offsize == this->n_words + 1);
		offsets.risk_set_data((offset_t*)(bbase + ac_str_blocks[0].offset));
		offsets.risk_set_size(offsize);
		offsets.risk_set_capacity(offsize);
	}
	if (base->ac_word_ext >= 2) {
		size_t poolsize = size_t(ac_str_blocks[1].length);
		assert(poolsize == offsets.back());
		strpool.risk_set_data((char*)(bbase + ac_str_blocks[1].offset));
		strpool.risk_set_size(poolsize);
		strpool.risk_set_capacity(poolsize);
	}
}

void BaseAC::ac_str_prepare_save_mmap(DFA_MmapHeader* base, const void** dataPtrs)
const {
	assert(base->num_blocks > 0);
	auto blocks_end = &base->blocks[base->num_blocks];
	base->ac_word_ext = 0;
	if (this->has_word_length()) {
		blocks_end[0].offset = align_to_64(blocks_end[-1].endpos());
		blocks_end[0].length = sizeof(word_id_t)* offsets.size();
		dataPtrs[base->num_blocks++] = offsets.data();
		base->ac_word_ext = 1;
	}
	if (this->has_word_content()) {
		blocks_end[1].offset = align_to_64(blocks_end[0].endpos());
		blocks_end[1].length = sizeof(char)* strpool.size();
		dataPtrs[base->num_blocks++] = strpool.data();
		base->ac_word_ext = 2;
	}
	base->dawg_num_words = n_words;
}

void BaseAC::assign(const BaseAC& y) {
	n_words = y.n_words;
	offsets = y.offsets;
	strpool = y.strpool;
}

void BaseAC::swap(BaseAC& y) {
	assert(typeid(*this) == typeid(y));
	std::swap(n_words, y.n_words);
	offsets.swap(y.offsets);
	strpool.swap(y.strpool);
}

} // namespace terark

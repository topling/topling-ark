#include "dict_trie.hpp"
#include "tmplinst.hpp"
#include "dfa_mmap_header.hpp"


namespace terark {

DictTrie::DictTrie() {
	m_numWords = 0;
	this->m_is_dag = true;
}

DictTrie::~DictTrie() {
}

std::pair<size_t, bool>
DictTrie::trie_add_word(size_t root, fstring word) {
	size_t curr = root;
	size_t i = 0;
	for(; i < word.size(); ++i) {
		size_t next = this->state_move(curr, word[i]);
		if (this->nil_state == next)
			goto AddNewWord;
		else
			curr = next;
	}
	{
		bool existed = this->is_term(curr);
		if (!existed) {
			if (m_numWords >= state_t::max_mapid) {
				THROW_STD(out_of_range,
						"m_numWords=%ld", long(m_numWords));
			}
			//this->set_term_bit(curr);
			this->states[curr].mapid = m_numWords++;
		}
		return std::make_pair(curr, !existed);
	}
AddNewWord:
	if (m_numWords >= state_t::max_mapid) {
		THROW_STD(out_of_range,
				"m_numWords=%ld", long(m_numWords));
	}
	for (; i < word.size(); ++i) {
		size_t next = this->new_state();
		this->add_move(curr, next, word[i]);
		curr = next;
	}
	this->states[curr].mapid = m_numWords++;
	return std::make_pair(curr, true);
}

template<class DataIO>
void DataIO_loadObject(DataIO& dio, DictTrie&) {
	THROW_STD(invalid_argument, "Method Not Implemented");
}
template<class DataIO>
void DataIO_saveObject(DataIO& dio, const DictTrie&) {
	THROW_STD(invalid_argument, "Method Not Implemented");
}

TMPL_INST_DFA_CLASS(DictTrie)

} // namespace terark


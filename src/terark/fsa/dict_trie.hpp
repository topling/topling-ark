#pragma once

#include <terark/fsa/automata.hpp>

namespace terark {
#include <terark/fsa/ppi/trie_map_state.hpp>

class DictTrie : public Automata<TrieMapState> {
	size_t m_numWords;

public:
	DictTrie();
	~DictTrie();
	size_t num_words() const { return m_numWords; }

	std::pair<size_t, bool>
	trie_add_word(fstring word) { return trie_add_word(initial_state, word); }

	std::pair<size_t, bool>
	trie_add_word(size_t root, fstring word);

	size_t get_mapid(size_t s) const {
		assert(s < this->states.size());
		return this->states[s].mapid;
	}
};

} // namespace terark

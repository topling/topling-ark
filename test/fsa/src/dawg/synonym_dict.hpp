#ifndef __cloudmark_synonym_dict_h__
#define __cloudmark_synonym_dict_h__

#include <terark/fsa/dawg.hpp>

namespace terark {

class SynonymDict {
public:
	typedef DAWG<State32> syno_index_t;
	syno_index_t  m_index;
	valvec<char>  m_strpool;
	valvec<uint>  m_offsets; // offset to strpool
	valvec<uint>  m_setlist; // synonyms are cyclic linked list

	SynonymDict();
	~SynonymDict();

	size_t syno_index(fstring word) const {
		if (m_index.num_words() == 0)
			return m_index.null_word;
		else
			return m_index.index(word);
	}

	template<class OnMatch, class Translator>
    size_t syno_match_words(fstring r, OnMatch on_match, Translator tr) const {
		if (m_index.num_words() == 0)
			return 0;
		else
			return m_index.tpl_match_dawg(r, on_match, tr);
	}

	void swap(SynonymDict& y);
	void append_text(char delim, const char* fname) const;
	void load_text(char delim, FILE* f);
	void save_text(char delim, FILE* f) const;

	void load_text(char delim, const char* fname);
	void save_text(char delim, const char* fname) const;

	void del_bad(char delim, FILE* tmpFile) const;

	void load_binary(const char* fname);
	void save_binary(const char* fname) const;
};

} // namespace terark

#endif // __cloudmark_synonym_dict_h__


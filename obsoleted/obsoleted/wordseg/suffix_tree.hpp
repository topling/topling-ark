/* vim: set tabstop=4 : */
#ifndef __terark_wordseg_suffix_tree_h__
#define __terark_wordseg_suffix_tree_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#include <string>
#include <vector>
#include "../const_string.hpp"
#include "../suffix_tree.hpp"
#include "TokenWord.hpp"

namespace terark { namespace wordseg {

class TERARK_DLL_EXPORT SuffixTree_WordSeg
{
	typedef std::vector<const_wstring>::const_iterator citer_t;
	typedef std::vector<const_wstring>::iterator iter_t;

public:
	SuffixTree_WordSeg(int min_word_len, int max_word_len);

	int min_word_len() const { return m_min_word_len; }

	std::vector<const_wstring>::const_iterator t_begin() const { return m_tree.begin(); }
	std::vector<const_wstring>::const_iterator t_end() const { return m_tree.end(); }

	size_t t_size() const { return m_tree.size(); }
	const std::vector<const_wstring>& tree() const { return m_tree; }

	void prepare(const wchar_t* first, const wchar_t* last);
	void prepare(const const_wstring text) { prepare(text.begin(), text.end()); }

	std::vector<const_wstring> all_word_pos(int minFreq=2);

	std::vector<TokenWord> forward_word_pos(int minFreq=2);
	std::vector<TokenWord> forward_word_pos2(int minFreq=2);
	std::vector<TokenWord> forward_word_pos3(int minFreq=2);

	void prepare_bw(const wchar_t* first, const wchar_t* last);
	void prepare_bw(const const_wstring text) { prepare_bw(text.begin(), text.end()); }
	std::vector<TokenWord> backward_word_pos(int minFreq=2);

	std::vector<TokenWord> bidirect_word_pos(const const_wstring text, int minFreq=2);

	std::vector<TokenWord>
		bidirect_union_dict_seg(
			const std::vector<TokenWord>& dict_fore,
			const std::vector<TokenWord>& dict_back,
			const const_wstring text, int minFreq
			);

	//! @return vector<std::pair<word, area> > 是按 elem.second 反向排序的
	//! 每个词只会出现一次，elem.second 标明了它的出现次数乘以它的长度
	std::vector<TokenWord> get_word_area(int minFreq = 2);

	//! @return vector<std::pair<word, freq> > 是按 elem.second 反向排序的
	//! 每个词只会出现一次，elem.second 标明了它的出现次数
	std::vector<TokenWord> get_word_freq(int minFreq = 2);
	//@}

protected:
	void add_words(const wchar_t* first, const wchar_t* last);
	void add_words_bw(const wchar_t* first, const wchar_t* last);
	void all_word_pos_loop(int minFreq, std::vector<const_wstring>& words, int pos, citer_t first, citer_t last);
	void get_word_freq_loop(int minFreq, std::vector<TokenWord>& wordsFreq, int pos, citer_t first, citer_t last);

#if defined(_DEBUG) || !defined(NDEBUG)
	void debug_check();
#endif

protected:
	int m_min_word_len;
	int m_max_word_len;
	const_wstring m_text;
	std::vector<const_wstring> m_tree;
};

} }

#endif // __terark_wordseg_suffix_tree_h__

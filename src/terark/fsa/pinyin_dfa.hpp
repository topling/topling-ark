#pragma once

#include <terark/util/function.hpp>
#include <terark/config.hpp>
#include <terark/fstring.hpp>
#include <terark/util/fstrvec.hpp>

namespace terark {

class MatchingDFA;

class TERARK_DLL_EXPORT PinYinDFA_Builder {
	class Impl;
	Impl* m_impl;
public:
	PinYinDFA_Builder(fstring basepinyin_file);
	~PinYinDFA_Builder();
	void load_base_pinyin(fstring basepinyin_file);
	void get_hzpinyin(fstring hzword, fstrvec* hzpinyin) const;
	MatchingDFA* make_pinyin_dfa(fstring hzword, fstrvec* hzpinyin, size_t minJinPinLen = 5) const;

	typedef function<
		void(size_t keylen, size_t value_idx, fstring value)
	> OnMatchKey;
	static size_t match_pinyin(const MatchingDFA* db, const MatchingDFA* key, const OnMatchKey& on_match);
};

} // namespace terark

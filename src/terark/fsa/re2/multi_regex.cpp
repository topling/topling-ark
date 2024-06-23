#include "multi_regex.hpp"
#include <terark/fsa/aho_corasick.hpp>
#include <terark/fsa/double_array_trie.hpp>

using namespace re2;

typedef Aho_Corasick<DoubleArrayTrie<AC_State<DA_State8B> > > da_ac_t;
typedef Aho_Corasick<Automata       <AC_State<State32   > > > au_ac_t;

class MultiRegex::Impl {
public:
	da_ac_t              fast_ac;
	re2::FilteredRE2     regex_set;
};

MultiRegex::MultiRegex() {
	impl = new Impl;
}
MultiRegex::~MultiRegex() {
	delete impl;
}

re2::RE2::ErrorCode
MultiRegex::add(StringPiece pattern, const RE2::Options& options, int *id) {
	return impl->regex_set.Add(pattern, options, id);
}

void MultiRegex::compile() {
	std::vector<std::string> all_atoms;
	impl->regex_set.Compile(&all_atoms);
	au_ac_t  slow_ac;
	au_ac_t::ac_builder builder(&slow_ac);
	for (size_t i = 0; i < all_atoms.size(); ++i) {
		fstring atom = all_atoms[i];
		builder.add_word(atom) = i;
	}
	builder.compile();
	double_array_ac_build_from_au(impl->fast_ac, slow_ac, "DFS");
}

class LowerCaseInput {
public:
	const unsigned char *pos, *end;
	bool empty() const  { assert(pos <=end); return  pos == end; }
	unsigned char operator*() { assert(pos < end); return tolower(*pos); }
	void operator++() { ++pos; }
};

void MultiRegex::scan(StringPiece text, std::vector<int>* matched_regexs) {
	std::vector<int> matched_atoms;
	LowerCaseInput input;
	input.pos = (const unsigned char*)text.begin();
	input.end = (const unsigned char*)text.end();
	impl->fast_ac.scan_stream(input,
		[&](size_t, const unsigned* words, size_t num) {
			for (size_t i = 0; i < num; ++i)
				matched_atoms.push_back(words[i]);
		});
	auto new_end = std::unique(matched_atoms.begin(), matched_atoms.end());
	matched_atoms.erase(new_end, matched_atoms.end());
	matched_regexs->resize(0);
	impl->regex_set.AllMatches(text, matched_atoms, matched_regexs);
}

const RE2* MultiRegex::GetRE2(int id) const {
	return impl->regex_set.GetRE2(id);
}


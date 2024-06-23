#include "pinyin_dfa.hpp"
#include "automata.hpp"
#include "onfly_minimize.hpp"
#include <terark/util/autoclose.hpp>
#include <terark/util/linebuf.hpp>
#include <terark/util/unicode_iterator.hpp>
#include <terark/hash_strmap.hpp>

namespace terark {

typedef Automata<State32> TheDFA;

class PinYinDFA_Builder::Impl {
public:
	struct PinYinInfo {
		unsigned root   : 32;
		unsigned size   : 31;
		unsigned is_dan :  1;
		PinYinInfo(unsigned r, unsigned n, bool b) {
			root = r;
			size = n;
			is_dan = b ? 1 : 0;
		}
	};

	TheDFA m_hz2py; // utf8 hanzi to pinyin
	hash_strmap<PinYinInfo> m_hzmap;
	valvec<char>          m_danyin_pinyin;

public:
	void load_basepinyin_slow(fstring basepinyin_file);
	void load_basepinyin_fast(fstring basepinyin_file);
	void get_hzpinyin(fstring hzword, fstrvec* hzpinyin) const;
	MatchingDFA* make_pinyin_dfa_slow(fstring hzword, fstrvec* hzpinyin) const;
	MatchingDFA* make_pinyin_dfa_fast(fstring hzword, fstrvec* hzpinyin, size_t minJianPinLen) const;
};

PinYinDFA_Builder::PinYinDFA_Builder(fstring basepinyin_file) {
	m_impl = NULL;
	load_base_pinyin(basepinyin_file);
}

PinYinDFA_Builder::~PinYinDFA_Builder() {
	delete m_impl;
}

void PinYinDFA_Builder::load_base_pinyin(fstring basepinyin_file) {
	if (m_impl) {
		THROW_STD(invalid_argument, "basepinyin_file has been loaded");
	}
	m_impl = new Impl;
	m_impl->load_basepinyin_fast(basepinyin_file);
}

void PinYinDFA_Builder::get_hzpinyin(fstring hzword, fstrvec* hzpinyin) const {
	if (NULL == m_impl) {
		THROW_STD(invalid_argument, "FATAL: basepinyin is not loaded");
	}
	if (NULL == hzpinyin) {
		THROW_STD(invalid_argument, "FATAL: hzpinyin is NULL");
	}
	hzpinyin->erase_all();
	if (!hzword.empty())
		m_impl->get_hzpinyin(hzword, hzpinyin);
}

MatchingDFA*
PinYinDFA_Builder::make_pinyin_dfa(fstring hzword, fstrvec* hzpinyin, size_t minJianPinLen) const {
	if (NULL == m_impl) {
		THROW_STD(invalid_argument, "FATAL: basepinyin is not loaded");
	}
	if (hzword.empty()) return NULL;
	return m_impl->make_pinyin_dfa_fast(hzword, hzpinyin, minJianPinLen);
}

namespace {
struct IsDigit{ bool operator()(byte_t c) const {return ::isdigit(c) != 0; }};
struct IsSpace{ bool operator()(byte_t c) const {return ::isspace(c) != 0; }};
}

/////////////////////////////////////////////////////////////////////////////

void PinYinDFA_Builder::Impl::load_basepinyin_slow(fstring basepinyin_file) {
	terark::Auto_fclose fp(fopen(basepinyin_file.c_str(), "r"));
	if (!fp) {
		THROW_STD(logic_error,
				"fopen(\"%s\", \"r\") = %s", basepinyin_file.c_str(), strerror(errno));
	}
  {
	MinADFA_OnflyBuilder<TheDFA> onfly(&m_hz2py);
	terark::LineBuf line;
	while (line.getline(fp) > 0) {
		line.chomp();
		char* end = std::remove_if(line.begin(), line.end(), IsDigit());
		line.n = end - line.p;
		std::replace_if(line.begin(), line.end(), IsSpace(), '\t');
		line.n = end - line.p;
		onfly.add_word(fstring(line.p, line.n));
	}
  }
	m_hz2py.set_is_dag(true);
	m_hz2py.set_kv_delim('\t');
//	m_hz2py.print_output("pinyin_dfa-basepinyin.txt");
}

void PinYinDFA_Builder::Impl::load_basepinyin_fast(fstring basepinyin_file) {
	terark::Auto_fclose fp(fopen(basepinyin_file.c_str(), "r"));
	if (!fp) {
		THROW_STD(logic_error,
				"fopen(\"%s\", \"r\") = %s", basepinyin_file.c_str(), strerror(errno));
	}
	terark::LineBuf line;
	hash_strmap<valvec<char> > hz_to_py;
	while (line.getline(fp) > 0) {
		line.trim();
		char* end = std::remove_if(line.begin(), line.end(), IsDigit());
		line.n = end - line.p;
		std::replace_if(line.begin(), line.end(), IsSpace(), '\t');
		line.n = end - line.p;
	//	typedef terark::u8_to_u32_iterator<const char*> u8iter;
		size_t ulen = terark::utf8_byte_count(*line.p);
		if (line[ulen] == '\t') {
			hz_to_py[fstring(line.p, ulen)].append(line.p + ulen, line.end());
		//	valid_lines++;
		} else {
			fprintf(stderr, "ERROR: invalid line: %s\n", line.p);
		}
	}
	terark::AutoFree<CharTarget<size_t> > children(TheDFA::sigma);
	TheDFA dfa1, dfa2;
	MinADFA_OnflyBuilder<TheDFA> onfly(&dfa1);
	valvec<fstring> F;
	for(size_t i = 0; i < hz_to_py.end_i(); i++) {
		fstring hanzi = hz_to_py.key(i);
		fstring py = hz_to_py.val(i);
		py.split(' ', &F);
		assert(F.size() >= 1);
		if (F.size() > 1) {
			dfa1.erase_all();
			onfly.reset_root_state(initial_state);
			for (size_t j = 0; j < F.size(); ++j) onfly.add_word(F[j]);
			onfly.close();
			if (dfa1.total_states() < 2) {
				fprintf(stderr, "ERROR: hanzi: %.*s pinyin: %.*s\n",
						hanzi.ilen(), hanzi.data(), py.ilen(), py.data());
			} else {
				dfa2.normalize(dfa1, "DFS");
				size_t root = m_hz2py.total_states();
				m_hz2py.resize_states(root + dfa2.total_states());
				for(size_t j = 0; j < dfa2.total_states(); ++j) {
					size_t n = dfa2.get_all_move(j, children);
					for(size_t k = 0; k < n; ++k) children[k].target += root;
					m_hz2py.add_all_move(root + j, children, n);
					if (dfa2.is_term(j))
						m_hz2py.set_term_bit(root + j);
				}
				auto ib = m_hzmap.insert_i(hanzi, PinYinInfo(root, dfa2.total_states(), 0));
				if (!ib.second) {
					THROW_STD(runtime_error, "Unexpected");
				}
			}
		}
		else {
			auto offset = m_danyin_pinyin.size();
			auto ib = m_hzmap.insert_i(hanzi, PinYinInfo(offset, F[0].n, 1));
			if (!ib.second) {
				THROW_STD(runtime_error, "Unexpected");
			}
			m_danyin_pinyin.append(F[0].p, F[0].n);
		}
	}
	m_hz2py.set_is_dag(true);
	m_hz2py.shrink_to_fit();
	m_danyin_pinyin.shrink_to_fit();
//	fprintf(stderr, "m_danyin_offset.size=%zd\n", m_danyin_offset.size());
}

MatchingDFA*
PinYinDFA_Builder::Impl::make_pinyin_dfa_slow(fstring hzword, fstrvec* hzpinyin) const {
	std::unique_ptr<TheDFA> dfa(new TheDFA());
	TheDFA tmpdfa;
	MinADFA_OnflyBuilder<TheDFA> onfly(&tmpdfa);
	valvec<size_t> roots(hzword.size()/2, valvec_reserve());
	for(size_t i = 0; i < hzword.size(); ) {
		size_t ulen = terark::utf8_byte_count(hzword[i]);
		if (i + ulen > hzword.size()) {
			THROW_STD(invalid_argument, "Invalid UTF8 char");
		}
		onfly.reset_root_state(initial_state);
		auto on_match = [&](size_t keylen, size_t, fstring pinyin) {
			if (keylen != ulen) {
			//	THROW_STD(invalid_argument,
			//		"error keylen=%zd utf8_len=%zd", keylen, ulen);
			//	fprintf(stderr, "matched invalid utf8: %.*s | val=%.*s\n",
			//		(int)keylen, hzword.p+i, pinyin.ilen(), pinyin.data());
			} else {
				onfly.add_key_val('\t', pinyin, "");
			}
		};
		m_hz2py.tpl_match_key_l('\t', hzword.substr(i), on_match);
		if (tmpdfa.total_states() <= 1) {
		//	THROW_STD(invalid_argument, "not found pinyin for hanzi: %.*s",
		//		int(ulen), hzword.p+i);
		//	fprintf(stderr, "not found pinyin for hanzi: %.*s\n", int(ulen), hzword.p+i);
			return NULL;
		}
		size_t root = dfa->copy_from(tmpdfa);
		roots.push_back(root);
		i += ulen;
		tmpdfa.erase_all();
	}
	valvec<TheDFA::state_id_t> finalstates;
	NFA_BaseOnDFA<TheDFA> nfa(dfa.get());
	for (size_t i = 1; i < roots.size(); ++i) {
		dfa->get_final_states(roots[i-1], &finalstates);
		for (size_t j = 0; j < finalstates.size(); ++j) {
			nfa.push_epsilon(finalstates[j], roots[i]);
			dfa->clear_term_bit(finalstates[j]);
		}
	}
	nfa.push_epsilon(initial_state, roots[0]);
	nfa.make_dfa(&tmpdfa);
	tmpdfa.set_is_dag(true);
//	tmpdfa.write_dot_file("hanzi_to_pinyin_dfa.dot");
//	tmpdfa.print_output("hanzi_to_pinyin_dfa.txt");
	dfa->swap(tmpdfa);
	return dfa.release();
}

void
PinYinDFA_Builder::Impl::get_hzpinyin(fstring hzword, fstrvec* hzpinyin) const {
	for(size_t i = 0; i < hzword.size(); ) {
		size_t ulen = terark::utf8_byte_count(hzword[i]);
		if (i + ulen > hzword.size()) {
			THROW_STD(invalid_argument, "Invalid UTF8 char");
		}
		size_t idx = m_hzmap.find_i(hzword.substr(i, ulen));
	//	fprintf(stderr, "duoyin_idx=%zd danyin_idx=%zd\n", duoyin_idx, danyin_idx);
		if (m_hzmap.end_i() == idx) {
		//	fprintf(stderr, "Not Found: %.*s\n", int(ulen), hzword.p+i);
			hzpinyin->erase_all();
			return;
		}
		PinYinInfo v = m_hzmap.val(idx);
		if (v.is_dan) {
			fstring pinyin(m_danyin_pinyin.data() + v.root, v.size);
			hzpinyin->push_back(pinyin);
		//	fprintf(stderr, "%.*s\n", pinyin.ilen(), pinyin.data());
		}
		else {
			hzpinyin->push_back();
			auto on_onepinyin = [&](size_t, fstring onepinyin) {
				hzpinyin->back_append(onepinyin);
				hzpinyin->back_append('\t');
			};
			m_hz2py.for_each_word(v.root, 0, ref(on_onepinyin));
		//	src_dfa.print_output(stderr);
		}
		i += ulen;
	}
}

static void
add_single_pinyin(fstring pinyin, TheDFA& dfa, fstrvec* hzpinyin, bool withJianPin) {
	size_t root = dfa.total_states() - 1;
	size_t tail = dfa.total_states() + pinyin.size();
	dfa.resize_states(tail + 1);
	for(size_t k = 0; k < pinyin.size(); ++k) {
		byte_t c = (byte_t)pinyin[k];
		dfa.add_move(root+k, root+k+1, c);
	}
	dfa.add_move(tail-1, tail, '\t');
	if (withJianPin && pinyin.size() >= 2)
		dfa.add_move(root+1, tail, '\t');
	if (hzpinyin)
		hzpinyin->push_back(pinyin);
}

MatchingDFA*
PinYinDFA_Builder::Impl::make_pinyin_dfa_fast(fstring hzword, fstrvec* hzpinyin, size_t minJianPinLen) const {
	std::unique_ptr<TheDFA> dfa(new TheDFA());
	dfa->reserve(hzword.size() * 4);
	assert(dfa->total_states() == 1);
	terark::AutoFree<CharTarget<size_t> > children(dfa->get_sigma());
	if (hzpinyin) hzpinyin->erase_all();
	size_t hz_num = 0;
	bool is_all_hz = true;
	for(size_t i = 0; i < hzword.size(); ) {
		size_t ulen = terark::utf8_byte_count(hzword[i]);
		if (i + ulen > hzword.size()) {
			THROW_STD(invalid_argument, "Invalid UTF8 char");
		}
		hz_num++;
		i += ulen;
		if (1 == ulen)
			is_all_hz = false;
	}
	bool withJianPin = hz_num >= minJianPinLen && is_all_hz;
	for(size_t i = 0; i < hzword.size(); ) {
		size_t ulen = terark::utf8_byte_count(hzword[i]);
		if (1 == ulen) {
			size_t j = i;
			for (; j < hzword.size() && hzword[j] < 128; ++j) {
				if (!isalnum(hzword[j])) return NULL;
			}
			add_single_pinyin(fstring(hzword.p+i, j-i), *dfa, hzpinyin, withJianPin);
			i = j;
			continue;
		}
		size_t idx = m_hzmap.find_i(hzword.substr(i, ulen));
		if (m_hzmap.end_i() == idx) {
		//	fprintf(stderr, "Not Found: %.*s\n", int(ulen), hzword.p+i);
			return NULL;
		}
		PinYinInfo v = m_hzmap.val(idx);
		if (v.is_dan) {
			fstring pinyin(m_danyin_pinyin.data() + v.root, v.size);
		//	fprintf(stderr, "%.*s\n", pinyin.ilen(), pinyin.data());
			add_single_pinyin(pinyin, *dfa, hzpinyin, withJianPin);
		}
		else {
			size_t src_root = v.root;
			size_t src_size = v.size;
			size_t root = dfa->total_states() - 1;
			size_t tail = dfa->total_states() + src_size - 1;
			dfa->resize_states(tail + 1);
			for(size_t k = 0; k < src_size; ++k) {
				assert(!m_hz2py.is_free(src_root + k));
				size_t n = m_hz2py.get_all_move(src_root + k, children);
				for (size_t j = 0; j < n; ++j) children[j].target += root - src_root;
				if (m_hz2py.is_term(src_root + k)) {
					auto end = children.p + n;
					auto p = lower_bound_ex(children.p, n, '\t', CharTarget_By_ch());
					if (p != end && '\t' == p->ch) {
						THROW_STD(runtime_error, "Unexpected: '\t' == p->ch");
					}
					for (auto q = end; q > p; --q) q[0] = q[-1];
					p->ch = '\t';
					p->target = tail;
					n++;
				}
				dfa->add_all_move(root + k, children, n);
			}
			if (withJianPin) {
				size_t n = dfa->get_all_move(root, children);
				for (size_t j = 0; j < n; ++j)
					dfa->add_move(children[j].target, tail, '\t');
			}
			if (hzpinyin) {
				hzpinyin->push_back();
				auto on_onepinyin = [&](size_t, fstring onepinyin) {
					hzpinyin->back_append(onepinyin);
					hzpinyin->back_append('\t');
				};
				m_hz2py.for_each_word(src_root, 0, ref(on_onepinyin));
			}
		//	src_dfa.print_output(stderr);
		}
		i += ulen;
	}
	size_t most_final = dfa->total_states() - 1;
	dfa->set_term_bit(most_final);
	dfa->set_is_dag(true);
//	dfa->write_dot_file("hanzi_to_pinyin_dfa.dot.fast");
//	dfa->print_output("hanzi_to_pinyin_dfa.txt.fast");
	return dfa.release();
}

size_t PinYinDFA_Builder::match_pinyin
(const MatchingDFA* db, const MatchingDFA* key, const OnMatchKey& on_match)
{
	assert(NULL != db);
	assert(NULL != key);
	return db->match_dfakey_l('\t', *key, on_match);
}

} // namespace terark

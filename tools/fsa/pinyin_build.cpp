#define _CRT_SECURE_NO_WARNINGS
#define _SCL_SECURE_NO_WARNINGS

#include <terark/fsa/automata.hpp>
#include <terark/fsa/linear_dfa.hpp>
#include <terark/fsa/louds_dfa.hpp>
#include <terark/fsa/quick_dfa.hpp>
#include <terark/fsa/squick_dfa.hpp>
#include <terark/fsa/onfly_minimize.hpp>
#include <terark/num_to_str.hpp>
#include <terark/util/profiling.hpp>
#include <terark/util/autoclose.hpp>
#include <terark/util/linebuf.hpp>
#include <terark/util/unicode_iterator.hpp>
#include <terark/util/fstrvec.hpp>
#include <getopt.h>

#if defined(__DARWIN_C_ANSI) || defined(_MSC_VER)
	#define malloc_stats() (void)(0)
#else
	#include <malloc.h>
#endif

#ifdef __GNUC__
	#include <dlfcn.h>
#endif

using namespace terark;

//Makefile:LDFLAGS:-ldl

#define PINYIN_EARLY_REVERSE       1
#define PINYIN_ORDERED_SUB_BUILDER 1
#define UNION_ALL_EDIT_DISTANCE 1

#define DFA_Template Automata
typedef Automata<State32> MainDFA;
typedef Automata<State32> MainPzDFA;

void usage(const char* prog) {
	fprintf(stderr, R"EOS(Usage:
   %s Options [Input-TXT-File]

Description:
   Build pinyin-to-HanZiWord DFA from Input-HanZiWord-File,
   If Input-HanZiWord-File is omitted, use stdin.

Options:
   -h : Show this help infomation
   -q : Be quiet, don't print progress info
   -O Large-DFA-File : large, but pretty fast
   -o Small-DFA-File : small, but pretty slow, now deprecated
   -S Super-DFA-File : small, a little (smaller and slower) than Quick-DFA
   -U Louds-DFA-File : very small, very slow(~5x slower than Large-DFA)
   -u Louds-DFA-File : same as -U, but use RankSelect_IL
   -m [Save-As-MemMap]: Argment is optional, default is 1
       * If not specified this option or omitted the argument, use default(1)
       * If the argument is 0, it means "DO NOT Save-As-MemMap"
   -z MaxZpathTrieNum:MinZpathLen : default is 5:2
   -E u1:e1,u2:e2,u3:e3,...
      Build edit-distance keys into the result DFA
      u1:e1 indicate edit-distance 'e1' is tolerated for unicode char num 'u1'
      Root of edit-distance is dfa.state_move(initial_state, '\1')
   -H : With HanZiWord, the result DFA is not only searched by PinYin, but aslo
      the HanZiWord self, a HanZiWord may have satellite data(such as word-freq),
      this will enable searching the satellite data by HanZiWord
   -j MinJianPinLen : default is 7
      Build JianPin(just ShengMu) for HanZiWords which length is at least
      MinJianPinLen
   -p BasePinYin-File
      * BasePinYin should at least include all single HanZi's PinYin.
      * BasePinYin could include extra HanZiWord to PinYin pairs, this feature
        is for prevent auto-spell Multiple (Pinyin to HanZiWord) pairs.
   -2 : Allow for Double-ShengMu JianPin
        A JianPin of a HanZiWord should be All-Double-ShengMu or All-Single-ShengMu
   -w WarningType
      Only one WarningType(nohz) is supportted now. More warning types maybe
      added later.
      WarningTypes:
      * nohz : Warning when unicode chars of word are not all HanZi
)EOS"
		, prog);
}

// When LD_PRELOAD with google tcmalloc, return memory to system
static void ReleaseHeap() {
#ifdef __GNUC__
	void* dl = dlopen(NULL, RTLD_NOW);
	if (NULL == dl) {
		fprintf(stderr, "ReleaseHeap: dlopen(NULL, 0) = %s\n", dlerror());
		return;
	}
	void (*Release)() = (void(*)())dlsym(dl, "MallocExtension_ReleaseFreeMemory");
	if (NULL == Release) {
		fprintf(stderr, "ReleaseHeap: dlsym(\"MallocExtension_ReleaseFreeMemory\") = %s\n", dlerror());
	} else {
		fprintf(stderr, "ReleaseHeap: MallocExtension_ReleaseFreeMemory\n");
		Release();
	}
	dlclose(dl);
#else
	fprintf(stderr, "ReleaseHeap: non-gnu-c\n");
#endif
}

terark::profiling pf;

const char* finput_name = NULL;
const char* xdfa_file = NULL;
const char* ldfa_file = NULL;
const char* qdfa_file = NULL;
const char* sdfa_file = NULL;
const char* udfa_file = NULL;
const char* udfa_file_il = NULL;
const char* ldfa_walk_method = "PFS";
const char* base_pinyin_file = NULL;
bool be_quiet = false;
bool save_as_mmap = true;
bool write_text = false;
bool with_hanzi = false;
bool two_shengmu = false;
bool warn_nohz = false;
NestLoudsTrieConfig g_zp_conf;
size_t g_minZpathLen = 3;
valvec<int> max_edit_dist;
size_t min_jianpin_len = 7;

#if defined(_MSC_VER)
std::string demangled_name(const char* name) { return name; }
#else
#include <cxxabi.h>
//__cxa_demangle(const char* __mangled_name, char* __output_buffer, size_t* __length, int* __status);
std::string demangled_name(const char* name) {
	int status = 0;
	terark::AutoFree<char> realname(
		abi::__cxa_demangle(name, NULL, NULL, &status)
	);
	return std::string(realname.p);
}
#endif

template<class Ldfa, class Au>
void build_ldfa_from_au(const char* WalkMethod, Ldfa& ldfa, const Au& au) {
	ldfa.build_from(ldfa_walk_method, au);
}
template<class RankSelect, class Au>
void build_ldfa_from_au(const char* /*WalkMethod*/,
						GenLoudsDFA<RankSelect>& ldfa, const Au& au) {
	ldfa.build_from(au, g_zp_conf);
}

template<class Ldfa, class Au>
void do_save_linear_dfa(const Au& au, const char* foutput) {
	if (!be_quiet)
		fprintf(stderr, "%s\n", BOOST_CURRENT_FUNCTION);
	std::string ldfa_class = demangled_name(typeid(Ldfa).name());
	const char* ldc = ldfa_class.c_str();
	long long t1 = pf.now();
	Ldfa ldfa;
	build_ldfa_from_au(ldfa_walk_method, ldfa, au);
	ldfa.set_is_dag(true);
	long long t2 = pf.now();
	if (!be_quiet)
		fprintf(stderr, "%s: states=%zd mem_size=%zd time=%f's\n", ldc, ldfa.total_states(), ldfa.mem_size(), pf.sf(t1,t2));
	if (save_as_mmap) {
		if (!be_quiet)
			fprintf(stderr, "write (small) [%s] mmap file: %s\n", ldc, foutput);
		ldfa.save_mmap(foutput);
	} else {
		if (!be_quiet)
			fprintf(stderr, "write (small) [%s] norm file: %s\n", ldc, foutput);
		ldfa.save_to(foutput);
	}
	if (write_text) {
		au.print_output("output.txt.au");
		ldfa.print_output("output.txt.ldfa");
	}
}

template<class FastPzDFA>
void do_save_fast_dfa(const FastPzDFA& dfa) {
	if (!be_quiet)
		fprintf(stderr, "%s\n", BOOST_CURRENT_FUNCTION);
	if (save_as_mmap) {
		if (!be_quiet)
			fprintf(stderr, "write fast dfa mmap file: %s\n", xdfa_file);
		dfa.save_mmap(xdfa_file);
	}
	else {
		if (!be_quiet)
			fprintf(stderr, "write fast dfa file: %s\n", xdfa_file);
		dfa.save_to(xdfa_file);
	}
}
template<class FastPzDFA>
void save_fast_dfa_aux(const MainPzDFA& src, FastPzDFA*) {
	FastPzDFA dst; dst.copy_with_pzip(src);
	dst.set_is_dag(true);
	do_save_fast_dfa(dst);
}
void save_fast_dfa_aux(const MainPzDFA& src, MainPzDFA*) {
	do_save_fast_dfa(src);
}

void save_fast_dfa(const MainPzDFA& pz) {
	for(size_t expand = 1; expand < 512; expand *= 16) {
		size_t dmax = pz.total_states() * expand;
		try {
		#define save_fast_dfa_if_fits(State) \
			else if (dmax < State::max_state) { \
				save_fast_dfa_aux(pz, (DFA_Template<State>*)NULL); break; }
			if (0){}
			save_fast_dfa_if_fits(State4B)
			save_fast_dfa_if_fits(State5B)
			save_fast_dfa_if_fits(State6B)
			save_fast_dfa_if_fits(State32)
		#if TERARK_WORD_BITS >= 64 && defined(TERARK_INST_ALL_DFA_TYPES)
		  #if !defined(_MSC_VER)
			save_fast_dfa_if_fits(State34)
		  #endif
			save_fast_dfa_if_fits(State7B)
			save_fast_dfa_if_fits(State38)
		#endif
			fprintf(stderr
			  , "Fatal: SaveFastDFA: MainPzDFA.states=%zd expand=%zd\n"
			  , pz.total_states(), expand);
			return;
		}
		catch (const std::out_of_range&) {
			if (!be_quiet)
				fprintf(stderr, "SaveFastDFA: try larger state\n");
		}
	}
}

template<template<int,int> class Ldfa>
void save_linear_dfa(const MainPzDFA& pz, const char* foutput) {
	for(size_t expand = 4; expand < 512; expand *= 16) {
		size_t dmax = pz.total_states() * expand;
		try {
		#define save_linear_if_fit(StateBytes) \
			if (dmax < Ldfa<StateBytes, 256>::max_state) { \
				do_save_linear_dfa<Ldfa<StateBytes, 256> >(pz, foutput); \
				return; }
		#if defined(TERARK_INST_ALL_DFA_TYPES)
			save_linear_if_fit(2)
			save_linear_if_fit(3)
		#endif
			save_linear_if_fit(4)
			save_linear_if_fit(5)
		#if TERARK_WORD_BITS >= 64 && !defined(_MSC_VER)
			save_linear_if_fit(6)
		#endif
		#if TERARK_WORD_BITS >= 64 && defined(TERARK_INST_ALL_DFA_TYPES)
			save_linear_if_fit(7)
			save_linear_if_fit(8)
		#endif
			// Unexpected:
			fprintf(stderr
			  , "Fatal: SaveLinearDFA: MainPzDFA.states=%zd expand=%zd\n"
			  , pz.total_states(), expand);
			return;
		}
		catch (const std::out_of_range&) {
			if (!be_quiet)
				fprintf(stderr, "SaveLinearDFA: try larger state\n");
		}
	}
}

Automata<State32> zh_char_to_pinyin;
Automata<State32> zh_char_to_jianpin;

size_t utf8_chars(const char* beg, const char* end) {
	size_t n = 0;
	const char* p = beg;
	for (; p < end; ++n) {
		p += terark::utf8_byte_count(*p);
	}
	return n;
}

void load_zh_char_to_pinyin() {
	terark::Auto_fclose fp(fopen(base_pinyin_file, "r"));
	MinADFA_OnflyBuilder<Automata<State32> > build_pinyin(&zh_char_to_pinyin);
	MinADFA_OnflyBuilder<Automata<State32> > build_jianpin(&zh_char_to_jianpin);
	terark::LineBuf line;
	long lineno = 0;
	fprintf(stderr, "loading base_pinyin_file=%s\n", base_pinyin_file);
	std::string jianpin;
	while (line.getline(fp) >= 0) {
		lineno++;
		line.chomp();
		if (line.empty()) continue;
		size_t ulen = 0;
		char* p = line.p;
		char* e = line.end();
		for (; p < e; ++ulen) {
			if ('\t' == *p)
				break;
			p += terark::utf8_byte_count(*p);
		}
		if ('\t' != *p) {
			fprintf(stderr, "lineno=%ld: invalid: %s\n", lineno, line.p);
			continue;
		}
		size_t n_pinyin = 0;
		jianpin.assign(line.p, p+1-line.p); // hz_word + '\t'
		for (const char* q = p; q < e; ++q) {
			if (!isalnum(*q)) {
				jianpin.push_back(q[1]);
				n_pinyin++;
			}
		}
		if (n_pinyin == ulen) {
		#if PINYIN_EARLY_REVERSE
			std::reverse(jianpin.begin()+(p+1-line.p), jianpin.end());
		#endif
			build_jianpin.add_word(jianpin);
		} else {
			fprintf(stderr, "lineno=%ld: skiped adding jianpin=%s line=%s\n", lineno, jianpin.c_str(), line.p);
		}
		if (two_shengmu) {
			// 为求简单，如果是多字的词，简拼要么全是双声母，要么全是单声母
			jianpin.resize(p+1-line.p); // hz_word + '\t'
			n_pinyin = 0;
			size_t two_num = 0;
			for (const char* q = p; q < e; ++q) {
				if (!isalnum(*q)) {
					if (terark_fstrstr("ch\0sh\0zh", 8, q+1, 2)) {
						two_num++;
						jianpin.append(q+1, 2);
					} else {
						jianpin.append(q+1, 1);
					}
					n_pinyin++;
				}
			}
			if (two_num && n_pinyin == ulen) {
			#if PINYIN_EARLY_REVERSE
				std::reverse(jianpin.begin()+(p+1-line.p), jianpin.end());
			#endif
				build_jianpin.add_word(jianpin);
			}
		}
		e = std::remove_if(p+1, e, [](char c){return !isalnum(c);});
		e[0] = '\0';
		line.n = e - line.p;
		#if PINYIN_EARLY_REVERSE
			std::reverse(p+1, e);
		#endif
		build_pinyin.add_word(line);
	}
	zh_char_to_pinyin.set_is_dag(true);
	zh_char_to_pinyin.print_output("pinyin_reversed.txt");
	zh_char_to_jianpin.set_is_dag(true);
	zh_char_to_jianpin.print_output("jianpin_reversed.txt");
	fprintf(stderr, "loaded base_pinyin_file=%s\n", base_pinyin_file);
}

// elements in [n, c) are in valid T state
template<class T>
class valvecl : private valvec<T> {
public:
	~valvecl() { this->n = this->c; }

	void resize(size_t NewSize) {
		size_t OldCap = this->capacity();
		if (NewSize > OldCap) {
			this->ensure_capacity(NewSize);
			size_t NewCap = this->capacity();
			T* px = this->p;
			for (size_t i = OldCap; i < NewCap; ++i)
				new(px+i) T ();
		}
		// erase_all for newly added T object
		T* px = this->p;
		for (size_t i = this->n; i < NewSize; ++i) {
			// when NewSize < this->n, it's ok to do nothing
			px[i].erase_all();
		}
		this->n = NewSize;
	}
	void erase_all() { this->n = 0; }

	void pop_back() {
		assert(this->n > 0);
		this->n--;
	}

/* pontetially slow for operator= from x
	void push_back(const T& x) {
		if (this->n < this->c) {
			this->p[this->n] = x;
			this->n++;
		} else {
			resize(this->n+1);
			this->back() = x;
		}
	}
*/
	T& addtail() {
		if (this->n < this->c) {
			return this->p[this->n++];
		} else {
			resize(this->n+1);
			return this->p[this->n];
		}
	}

	T& wr(size_t idx) {
		if (idx >= this->size())
			resize(idx+1);
		return this->p[idx];
	}
	using valvec<T>::operator[];
	using valvec<T>::size;
	using valvec<T>::empty;
	using valvec<T>::begin;
	using valvec<T>::end;
};

struct MultiPath : fstrvec {
   	size_t root;
	valvec<unsigned> finals;
};
struct DAG_NFA : public valvecl<valvecl<MultiPath> > {};
//dag[curr_pos][zh_word_len][*] is a pinyin

int build(MainPzDFA& au) {
	if (!be_quiet)
		fprintf(stderr, "%s\n", BOOST_CURRENT_FUNCTION);
	long lineno = 0;
	long long t0, t1, t2, t3, t4;
	size_t input_bytes_all = 0;
	size_t max_dfa_module_size = 0;
	{
		MainDFA small_au, small_au2;
		valvec<size_t> q1, q2; // BFS queue
		valvec<size_t> backlink;
		valvec<byte_t> color;
		size_t ulen;
		NFA_BaseOnDFA<MainDFA> small_nfa(&small_au);
		MinADFA_OnflyBuilder<MainDFA> large_build(&au);
	#if PINYIN_ORDERED_SUB_BUILDER
		MinADFA_Module_OnflyBuilder<MainDFA>::Ordered small_build(&small_au);
	#else
		MinADFA_OnflyBuilder<MainDFA> small_build(&small_au);
	#endif
		PFS_GraphWalker<unsigned> walker;
		DAG_NFA dag, jian;
		terark::LineBuf line;
		t0 = pf.now();
		byte_t delim = '\t';

		auto add_dag_nfa = [&](const Automata<State32>& pinyin_dict, DAG_NFA& dag) {
			dag.erase_all();
			dag.resize(ulen);
			auto zh_end = std::find(line.begin(), line.end(), delim);
			auto p = line.begin();
			for (size_t i = 0; i < ulen; ++i) {
				fstring str(p, zh_end);
				pinyin_dict.match_key(delim, str,
					[&](size_t klen, size_t, fstring pinyin) {
					//	printf("i=%zd klen=%zd %.*s : %s\n", i, klen, int(zh_end-p), p, pinyin.p);
						size_t u_klen = 0;
						for (auto q = p; q < p + klen; ++u_klen)
							q += terark::utf8_byte_count(*q);
						dag[i].wr(u_klen-1).push_back(pinyin);
					});
				p += terark::utf8_byte_count(*p);
			}
			// A. prune auto pinyin of known word's pinyin
			//
			// 1) find unweighted shortest path by BFS
		//	fprintf(stderr, "lineno=%ld: %s\n", lineno, line.p);
			q1.erase_all();
			q1.push_back(0);
			color.resize(ulen+2);
			color.fill('0');
			color.back() = '\0';
			color[0] = '1';
			backlink.resize(ulen+1);
			backlink.fill(size_t(-1));
			while (!q1.empty()) {
				q2.erase_all();
				for(size_t i = 0; i < q1.size(); ++i) {
					size_t parent = q1[i];
					for(ptrdiff_t j = dag[parent].size()-1; j >= 0; --j) {
						size_t child = parent + j + 1;
					//	fprintf(stderr, "parent=%zd  child=%zd\n", parent, child);
						if (color[child] == '0') {
							color[child] = '1';
							backlink[child] = parent;
							q2.push_back(child);
						}
						if (child == ulen) {
							goto BFS_Done;
						}
					}
				}
				q1.swap(q2);
			}
			if (warn_nohz)
				fprintf(stdout, "lineno=%ld, has no path(ulen=%zd): %s\n", lineno, ulen, line.p);
			return false;
		BFS_Done:
			// 2) remove single zh_char pinyin on zh_word in the shortest path
			//         fchild means Farthest child
			for(size_t fchild = ulen; ;) {
				size_t parent = backlink[fchild];
				assert(parent < fchild);
				assert(size_t(-1) != parent); // must be connected
				if (fchild - parent > 1) { // a muti-hanzi word
					for(size_t j = 0; j < dag[parent].size()-1; ++j) {
						// delete non-longest edges of parent
						// the_delting_child = parent + j + 1
						dag[parent][j].erase_all();
					}
					for(size_t i = parent+1; i < fchild; ++i) {
						// delete all edges between (direct child of parent) and fchild
						for (size_t j = 0; j < dag[i].size(); ++j) {
							dag[i][j].erase_all();
						}
					}
				}
				if (0 == parent)
					goto PruneCompleted;
				fchild = parent;
			}
			assert(0);
		PruneCompleted:
			// B. build DFA modules
			for (size_t i = 0; i < ulen; ++i) {
				for (size_t j = 0; j < dag[i].size(); ++j) {
					// i is parent,  i+j+1 is child
					const MultiPath& pathes = dag[i][j];
					if (pathes.empty()) continue;
					auto root = dag[i][j].root = small_au.new_state();
					if (pathes.size() == 1) {
						small_au.add_word(root, pathes[0]);
						continue;
					}
					small_build.reset_root_state(root);
					for (size_t k = 0; k < pathes.size(); ++k) {
						small_build.add_word(pathes[k]);
					}
					small_build.close();
				}
			}
			// C. build NFA: connect DFA modules with epsilon
			//    1) compute final state set of each DFA module
			for (size_t i = 0; i < ulen; ++i) {
				for (size_t j = 0; j < dag[i].size(); ++j) {
					auto& x = dag[i][j];
					if (x.empty()) continue;
					small_au.get_final_states(x.root, &x.finals, walker);
				}
			}
		#if PINYIN_EARLY_REVERSE
			for (size_t parent = 0; parent < ulen; ++parent) {
				for (size_t j = 0; j < dag[parent].size(); ++j) {
					const size_t child = parent+j+1;
					const MultiPath& child_to_parent = dag[parent][j]; // reversed
					if (child_to_parent.empty()) continue;
					if (0 != parent) {
						for (unsigned tail : child_to_parent.finals)
							small_au.clear_term_bit(tail);
					}
					if (child == ulen) {
						// now child is the reversed starting point
						small_nfa.push_epsilon(initial_state, child_to_parent.root);
						continue;
					}
					for (const auto& childchild_to_child : dag[child]) {
						if (childchild_to_child.empty()) continue;
						for (unsigned tail : childchild_to_child.finals) {
							small_nfa.push_epsilon(tail, child_to_parent.root);
						}
					}
				}
			}
		#else
			for (size_t parent = 0; parent < ulen; ++parent) {
				for (size_t j = 0; j < dag[parent].size(); ++j) {
					const size_t child = parent+j+1;
					const MultiPath& from_parent = dag[parent][j];
					if (from_parent.empty()) continue;
					if (child == ulen) continue;
					for (unsigned tail : from_parent.finals) {
						for (const MultiPath& from_child : dag[child])
							if (!dst.empty())
								small_nfa.push_epsilon(tail, from_child.root);
						small_au.clear_term_bit(tail);
					}
				}
			}
			// D. connect initial_state to the beginning DFA modules
			for (size_t j = 0; j < dag[0].size(); ++j) {
				if (dag[0][j].empty()) continue;
				small_nfa.push_epsilon(initial_state, dag[0][j].root);
			}
		#endif
			return true;
		};
		terark::Auto_fclose finput;
		if (finput_name) {
			finput = fopen(finput_name, "r");
			if (NULL == finput) {
				fprintf(stderr, "FAIL: fopen(\"%s\", \"r\") = %s\n", finput_name, strerror(errno));
				exit(1);
			}
		}
		MainDFA ed_dfa, ed_dfa1, ed_dfa2, ed_dfa3;
		valvec<size_t> roots;
		while (line.getline(finput.self_or(stdin)) >= 0) {
			lineno++;
			line.chomp();
		#if !defined(NDEBUG)
			std::string line_backup(line.p, line.n);
		#endif
			if (line.empty()) continue;
			auto zh_end = std::find(line.begin(), line.end(), delim);
			auto p = line.begin();
			if ((byte_t)*p < '\t') {
				fprintf(stderr, "lineno=%ld: invalid char(hex=%02X)\n", lineno, (byte_t)*p);
				continue;
			}
			ulen = 0;
			for (; p < zh_end; ++ulen)
				p += terark::utf8_byte_count(*p);
			if (p != zh_end) {
				fprintf(stderr, "lineno=%ld: bad utf8 char\n", lineno);
				continue;
			}
			roots.erase_all();
			ed_dfa.erase_all();
			if (max_edit_dist.size() && (ulen >= max_edit_dist.size() || max_edit_dist[ulen] > 0)) {
				int max_ed = max_edit_dist.back();
				if (ulen < max_edit_dist.size()-1) {
					max_ed = max_edit_dist[ulen];
				}
				fstring key(line.p, zh_end);
			#if defined(UNION_ALL_EDIT_DISTANCE)
				ed_dfa1.make_edit_distance_dfa_ex(key, max_ed, &small_nfa.reset());
				ed_dfa1.compute_reverse_nfa(&small_nfa.reset());
				small_nfa.make_dfa(&ed_dfa);
				ed_dfa.fast_append('\1', "");
			#else
				ed_dfa1.make_edit_distance_dfa_ex(key, 1, &small_nfa.reset());
				ed_dfa1.compute_reverse_nfa(&small_nfa.reset());
				small_nfa.make_dfa(&ed_dfa3);
				ed_dfa3.fast_append('\1', "");
				roots.push_back(ed_dfa.copy_from(ed_dfa3));
				for (int ed = 2; ed <= max_ed; ++ed) {
					ed_dfa2.make_edit_distance_dfa_ex(key, ed, &small_nfa.reset());
					ed_dfa3.dfa_difference(ed_dfa2, ed_dfa1);
					ed_dfa3.compute_reverse_nfa(&small_nfa.reset());
					small_nfa.make_dfa(&ed_dfa3);
					ed_dfa3.fast_append((char)ed, "");
					roots.push_back(ed_dfa.copy_from(ed_dfa3));
					ed_dfa1.swap(ed_dfa2);
				}
			#endif
			}
			small_nfa.reset();
			bool pinyin_ok = add_dag_nfa(zh_char_to_pinyin, dag);
			if (pinyin_ok && ulen >= min_jianpin_len) {
				add_dag_nfa(zh_char_to_jianpin, jian);
			}
		//	printf("%s----------------\n", line.p);
			if (pinyin_ok) {
				small_nfa.make_dfa(&small_au2);
			#if ! PINYIN_EARLY_REVERSE
			//	small_au2.print_output("/dev/stdout");
			//	small_au2.adfa_minimize();
				small_au2.compute_reverse_nfa(&small_nfa.reset());
				small_nfa.make_dfa(&small_au2);
			#endif
			//	small_au2.print_output("/dev/stdout");
				std::reverse(line.begin(), line.end());
			#if defined(UNION_ALL_EDIT_DISTANCE)
				if (ed_dfa.total_states() > 1) {
					small_au.dfa_union(ed_dfa, small_au2);
					small_au.adfa_minimize(small_au2);
				}
			#else
				if (roots.size()) {
					roots.push_back(ed_dfa.copy_from(small_au2));
					small_au.dfa_union_n(ed_dfa, roots);
					small_au.adfa_minimize(small_au2);
				}
			#endif
				size_t root = au.copy_from(small_au2);
				large_build.add_suffix_adfa(line, delim, root);
			} else {
				std::reverse(line.begin(), line.end());
			#if defined(UNION_ALL_EDIT_DISTANCE)
				if (ed_dfa.total_states() > 1) {
					ed_dfa.adfa_minimize(small_au);
					size_t root = au.copy_from(small_au);
					large_build.add_suffix_adfa(line, delim, root);
				}
			#else
				if (roots.size()) {
					small_au.dfa_union_n(ed_dfa, roots);
					small_au.adfa_minimize(small_au2);
					size_t root = au.copy_from(small_au2);
					large_build.add_suffix_adfa(line, delim, root);
				}
			#endif
			}
			if (with_hanzi)
				large_build.add_word(line);

			if (lineno % 10000 == 0) {
				fprintf(stderr, "lineno=%ld, DFA[mem_size=%ld states=%ld transitions=%ld]\n"
						, lineno
						, (long)au.mem_size()
						, (long)au.total_states()
						, (long)au.total_transitions()
						);
			}
			input_bytes_all += line.n;
			max_dfa_module_size = std::max(small_au.total_states(), max_dfa_module_size);
		}
	}
	t1 = pf.now();
	fprintf(stderr, "lineno=%ld, DFA[mem_size=%ld states=%ld transitions=%ld]\n"
			, lineno
			, (long)au.mem_size()
			, (long)au.total_states()
			, (long)au.total_transitions()
			);
	fprintf(stderr, "read finput completed, time: read+insert=%f's\n", pf.sf(t0,t1));
	fprintf(stderr, "max_dfa_module_size=%zd\n", max_dfa_module_size);
	fprintf(stderr, "reversing the final dfa\n");
	{
		ReleaseHeap();
#if 1
		MainDFA au3;
		NFA_BaseOnDFA<MainDFA> big_nfa(&au3);
#else
		CompactReverseNFA<uint32_t> big_nfa;
#endif
		fprintf(stderr, "\tcompute_reverse_nfa... ");
		au.compute_reverse_nfa(&big_nfa);
		t2 = pf.now();
		fprintf(stderr, "completed, time=%f's\n", pf.sf(t1, t2));
		big_nfa.print_nfa_info(stderr, "\t");
		fprintf(stderr, "\tNFA_to_DFA_make_dfa... ");
		au.clear();
	//	ReleaseHeap();
		big_nfa.make_dfa(&au);
		t3 = pf.now();
		fprintf(stderr, "completed, time=%f's\n", pf.sf(t2, t3));
	}
	fprintf(stderr, "reversed the final dfa, totaltime=%f's\n", pf.sf(t1,t3));
	fprintf(stderr, "path_zip...\n");
	size_t non_path_zip_mem = au.mem_size();
	size_t non_path_zip_states = au.total_states();
	au.path_zip(ldfa_walk_method, g_minZpathLen);
	au.set_is_dag(true);
	t4 = pf.now();
	ReleaseHeap();
	size_t pathes;
	size_t length = au.dag_total_path_len(&pathes);
	fprintf(stderr, "text[rows=%ld bytes=%zd] dfa[mem_size=%zd non_path_zip_mem=%zd]\n"
			, lineno, input_bytes_all
			, au.mem_size()
			, non_path_zip_mem
			);
	fprintf(stderr, "states: non_path_zip=%zd path_zip=%zd\n", non_path_zip_states, au.total_states());
	fprintf(stderr, "pathes: the total (pinyin, hanzi_word) pairs: %zd\n", pathes);
	fprintf(stderr, "length: length of such all pathes: %zd\n", length);
	fprintf(stderr, "text_file_size: length+pathes=%zd\n", length + pathes);
	fprintf(stderr, "time: path_zip=%f total=%f\n", pf.sf(t3,t4), pf.sf(t0,t4));
#ifdef __GNUC__
	malloc_stats();
#endif
	return 0;
}

// arg format: u1:e1,u2:e2,u3:e3
// arg sample: 4:1,6:2,10:3
void parse_edit_dist_arg(const char* arg) {
	max_edit_dist.erase_all(); // last -E arg take effect
	int prev_ed = 0;
	for(const char* p = optarg; *p; ) {
		const char* t = strchr(p+0, ':');
		if (t) {
			int utf8_cnt = atoi(p);
			int curr_ed = atoi(t+1);
			if (utf8_cnt < (int)max_edit_dist.size()) {
				fprintf(stderr,
						"ERROR: invalid -E argument, should be -E u1:e1,u2:e2,u3:e3...,"
						" must be u[i] < u[i+1] && e[i] < u[i+1] \n");
				exit(1);
			}
			for (int i = max_edit_dist.size(); i < utf8_cnt; ++i) {
				max_edit_dist.push_back(prev_ed);
			}
			prev_ed = curr_ed;
			max_edit_dist.push_back(curr_ed);
		} else {
			break;
		}
		const char* q = strchr(t+1, ',');
		if (q)
			p = q + 1;
		else
			break;
	}
	if (max_edit_dist.empty()) {
		fprintf(stderr,
				"ERROR: invalid -E argument, should be -E u1:e1,u2:e2,u3:e3...,"
				" must be u[i] < u[i+1] && e[i] < u[i+1] \n");
		exit(1);
	}
	if (max_edit_dist.back() <= 0) {
		fprintf(stderr, "ERROR: invalid -E argument, should be -E u1:e1,u2:e2,u3:e3...\n");
		exit(1);
	}
}

int main(int argc, char* argv[]) {
	g_zp_conf.initFromEnv();
	for (;;) {
		int opt = getopt(argc, argv, "E:o:O:tp:Hj:U:u:S:2hz:w:");
		switch (opt) {
		default:
		case '?':
		case 'h':
			usage(argv[0]);
			return 3;
		case -1:
			goto GetoptDone;
		case 'E': // last -E arg take effect
			parse_edit_dist_arg(optarg);
			break;
		case 'o':
			ldfa_file = optarg;
			break;
		case 'O':
			xdfa_file = optarg;
			break;
		case 'p':
			base_pinyin_file = optarg;
			break;
		case 't':
			write_text = true;
			break;
		case 'H':
			with_hanzi = true;
			break;
		case 'j':
			min_jianpin_len = atoi(optarg);
			break;
		case '2':
			two_shengmu = true;
			break;
		case 'U':
			udfa_file = optarg;
			break;
		case 'u':
			udfa_file_il = optarg;
			break;
		case 'S':
			sdfa_file = optarg;
			break;
		case 'z':
		  {
			const char* colon = strchr(optarg, ':');
			g_zp_conf.nestLevel = atoi(optarg);
			if (colon) {
				g_minZpathLen = strtoul(colon+1, NULL, 10);
				g_minZpathLen = std::max<size_t>(g_minZpathLen, 2);
				g_minZpathLen = std::min<size_t>(g_minZpathLen, 8);
			}
			else if (g_zp_conf.nestLevel > 1) {
				g_minZpathLen = 3;
			}
			break;
		  }
		case 'w':
			if (strcasecmp(optarg, "nohz") == 0) {
				warn_nohz = true;
			}
			break;
		}
	}
GetoptDone:
	if (optind < argc) {
		finput_name = argv[optind];
	}
	if (NULL == base_pinyin_file) {
		fprintf(stderr, "-p base_pinyin_file is required\n");
		usage(argv[0]);
		return 3;
	}
	if (!xdfa_file && !ldfa_file && !qdfa_file && !sdfa_file && !udfa_file) {
		fprintf(stderr, "At least one of\n"
			"\t-O xfast_dfa_file or -o small_linear_dfa_file or\n"
			"\t-Q quick_dfa_file or -S small_quick__dfa_file is required\n"
			"\t-U ultra_dfa_file\n"
			"is required\n");
		usage(argv[0]);
		return 3;
	}
	load_zh_char_to_pinyin();
	int ret = 0;
	try {
		MainPzDFA pz;
		ret = build(pz);
		if (xdfa_file)
			save_fast_dfa(pz);
		if (ldfa_file)
			save_linear_dfa<LinearDFA>(pz, ldfa_file);
		if (qdfa_file)
			save_linear_dfa< QuickDFA>(pz, qdfa_file);
		if (sdfa_file)
			save_linear_dfa<SQuickDFA>(pz, sdfa_file);
		if (udfa_file)
			do_save_linear_dfa<LoudsDFA_SE>(pz, udfa_file);
		if (udfa_file_il)
			do_save_linear_dfa<LoudsDFA_IL>(pz, udfa_file_il);
	}
	catch (const std::exception& ex) {
		fprintf(stderr, "failed: caught exception: %s\n", ex.what());
		ret = 1;
	}
	if (!be_quiet)
		malloc_stats();
	return ret;
}


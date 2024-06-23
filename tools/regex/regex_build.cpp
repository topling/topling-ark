#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#define _CRT_NONSTDC_NO_WARNINGS
#define _SCL_SECURE_NO_WARNINGS
#endif
#include <terark/fsa/re2/vm_nfa.hpp>
#include <terark/fsa/nfa.hpp>
#include <terark/fsa/automata.hpp>
#include <terark/fsa/dense_dfa.hpp>
#include <terark/fsa/dense_dfa_v2.hpp>
#include <terark/fsa/mre_delim.hpp>
#include <terark/fsa/mre_match.hpp>
#include <terark/fsa/virtual_machine_dfa_builder.hpp>
#include <terark/util/linebuf.hpp>
#include <terark/util/profiling.hpp>
#include <terark/util/fstrvec.hpp>
#include <terark/io/FileStream.hpp>
#include <boost/smart_ptr/intrusive_ptr.hpp>
#include <re2/re2.h>
#include <re2/prog.h>
#include <re2/regexp.h>
#include <getopt.h>
#include <random>

#if defined(_WIN32) || defined(_WIN64)
#include <io.h>
#undef ERROR
#ifndef STDERR_FILENO
#define STDERR_FILENO 2
#endif
#else
#include <unistd.h>
#endif
#include <fcntl.h>

//Makefile:CXXFLAGS: -I../../3rdparty/re2 -I../../3rdparty/re2/re2 -I../../3rdparty/re2/util

using namespace re2;
using namespace terark;

#define LOG_printf(type, format, ...) \
	if (stderr_is_tty) \
		fprintf(stderr, "line:%ld:" type ": \33[1;31m" format \
				"\33[0m\n", lineno, ##__VA_ARGS__); \
	else \
		fprintf(stderr, "line:%ld:" type ": " format \
				"\n", lineno, ##__VA_ARGS__)

#define WARN(format, ...) LOG_printf("Warning", format, ##__VA_ARGS__)
#define ERROR(format, ...) LOG_printf("Error", format, ##__VA_ARGS__)
#define FATAL(format, ...) \
  do { \
    LOG_printf("Fatal", format, ##__VA_ARGS__); \
    exit(5); \
  } while (0)

#define HIGH_LIGHT(format, ...) \
	if (stderr_is_tty) \
		fprintf(stderr, "\33[1;31m" format "\33[0m\n", ##__VA_ARGS__); \
	else \
		fprintf(stderr, format "\n", ##__VA_ARGS__)

#define dim_of(array) (sizeof(array) / sizeof(array[0]))

struct DfaPart {
	size_t root;
	size_t size;
};

template<class MyDFA>
struct ComposedRegex {
	MyDFA dfa;
	boost::intrusive_ptr<ComposedRegex> sub[2];
	long refcnt = 0;
	long pos_beg = -1;
	long pos_end = -1;
	long rng_min = -1;
	long rng_max = -1;
	bool is_evaled = false;
	char op; // & ! | . ? * + {  //}

	explicit ComposedRegex(char op) : op(op) {}

	bool is_leaf() const { return !sub[0] && !sub[1]; }

	ComposedRegex* clone() const {
		assert(this->refcnt > 0);
		ComposedRegex* y = new ComposedRegex(*this);
		y->refcnt = 0;
		return y;
   	}

	friend void intrusive_ptr_add_ref(ComposedRegex* p) { p->refcnt++; }
	friend void intrusive_ptr_release(ComposedRegex* p) {
		assert(p->refcnt > 0);
	   	if (0 == --p->refcnt) delete p;
   	}
};

template<class MyDFA>
using ComposedRegexPtr = std::unique_ptr<ComposedRegex<MyDFA> >;

class MultiRegexBuilder {
public:

size_t one_regex_timeout_us = 1*1000*1000;
size_t all_regex_timeout_seconds = 0;
int flags = Regexp::LikePerl | Regexp::MatchNL;
int add_dot_star = 0;
bool be_quiet = false;
bool write_dot_graph = false;
bool write_dot_graph_unioned = false;
bool dynamic_dfa_matching = false;
bool ignore_case = false;
bool do_print_parse_tree = getenv("REGEX_BUILD_PRINT_PARSE_TREE") ? 1 : 0;
bool limit_cluster_union_power_size = true;
int g_minZpathLen = 2;

// when enable with_submatch_capture
//  1. bin_meta_file should likely be used together
//  2. if bin_meta_file is not used, the result DFA is all-in-text
int with_submatch_capture = 0; // just for one-pass regex

// for efficiency, use binary value for key-val match
//  1. bin_meta_file is required for submatch capture
//     * submatch capture need to update matching position frequently
//     * access to matching position storage(an int32) must be very fast
//     * bin_meta_file is used to create MultiRegexSubmatch
//     * MultiRegexSubmatch is the storage for matching positions
//  2. bin_meta_file could also be used without submatch capture
//     * in this case, MultiRegexSubmatch could also be used for full match
//     * because there is no any submatch capture, this will be more faster
//  3. bin_meta_file is specified by option: -b bin_meta_file

// bin_meta_file format:
//   regex_id \t num_submatch \t IsOnePass \t Original Regex File Line
// regex_id is the 0-based regex index for fast access
// num_submatch includes the full match: submatch[0]
// so num_submatch is the number of capture groups
// the full match's capture point id is (0, 1)
// n'th  submatch's capture point id is (2*n, 2*n+1)
const char* bin_meta_file = NULL;
terark::FileStream bin_meta_fp;

const char* out_dfa_file = NULL;
const char* out_min_file = NULL;
const char* conflict_report_file = NULL;
char* enumConstBaseFileName = NULL;
char* enumConstNamespace = NULL;
hash_strmap<valvec<int> > enumConstMap;
terark::Auto_fclose finput;

#if defined(_MSC_VER)
bool stderr_is_tty = false;
#else
bool stderr_is_tty = isatty(STDERR_FILENO);
#endif

bool force_capture;
bool is_composing_regex;
bool is_onepass;
bool do_capture;

int sub_regex_idx;

// a submatch has 2 capture point
// n_submatch includes the full match
int n_submatch;

long lineno = 0;

valvec<size_t> roots1; ///< TODO: need a better name
valvec<size_t> dynaRoots1;
valvec<size_t> dynaRoots2;

void usage(const char* prog) const {
	fprintf(stderr, R"EOS(Usage:
   %s Options [Input-Regex-File]

Description:
   Compile multiple regular expressions into one DFA

Options:
   -h : Show this help infomation
   -q : Be quiet, don't print progress info
   -I : Ignore case for regex
   -L : Use Latin1 charset, not utf8(the default charset)
   -a Add-DotStar(Optional): can be 0, 1, 2
      0: Do not add DotStar, treat all regular expression as head-anchored
         The result DFA will be a "RegexMatch" DFA, not a "RegexSearch" DFA.
      1: Respect the regex's head anchor
      2: Prepend DotStar on the unioned DFA, this is just for **DEBUG TEST**
      Note:
         * Head anchored regex example: "^Hello\s+World!"
         * If this option is omitted, Add-Dot-Star is 0
         * If the option argument is omitted, Add-Dot-Star is 1
   -O Large-DFA-File: Large, but pretty fast
   -o Small-DFA-File: Small(maybe ~10x smaller than Large-DFA)
      Small-DFA is essentially a kind of Virtual Machine DFA.
      Small-DFA is not only small, but also fast, in most cases, it is as
      faster as Large-DFA. But it may be much faster than Large-DFA in some
      special/ideal cases.
   -b Bin-Meta-File: The meta file used for capturing submatch
   -s Optional-Arg
      Build dfa with submatch capture, but the dfa algorithm can only capture
      one-pass DFA.
      If Optional-Arg is 's', the algorithm will ignore the regex's one-pass
      property, and try to use one-pass capturing algorithm to capture all
      submatches, this may produce bad result. But when using utf8 encoding,
      some unicode-one-pass regex is NOT byte-one-pass, for example:
        "从([^到]+)到([^怎]+)怎么走" is unicode-one-pass but not byte-one-pass.
      By using option "-ss", MultiRegexSubmatch::match_utf8() will successfully
      capture the submatches.
      Prepend a '*' at the start of a line has the same effect of '-ss' for the
      current regex.
   -E C-Program-HeaderFile
      Generate enum constant definitions in C-Program-HeaderFile
	  constant name is specified by second column of Input-Regex-File
   -D : Build a dynamic matching DFA
      In many cases, the full unioned dfa can not be built, in this situation,
      dynamic matching DFA is a compromised solution between full-nfa matching
      and full-dfa matching.
      For speed up online dynamic matching, this program use a heuristic algorithm
      to clustering(partial union) SubDFAs offline.
   -z MinZpathLen, default is 2, only effective for full-unioned-dfa
   -g : Write DFA and NFA dot graph file for every regex
   -G : Write dot graph file for unioned DFA
   -c Conflict-Report-File
   -t DFA-Type: can be 'd', '1', '2', default is 'd'
      a: use DFA class which optimized for adfa_build
      d: use DenseDFA
      2: use DenseDFA_V2
   -T Timeout1[:Timeout2]: Timout2 is optional
      Timeout1: Timeout for compiling one regex
      Timeout2: Timeout for union all regex
   -P : DO NOT Limit cluster_union_power_size, default is true

Input-Regex-File format
   This is a tab separated text file, column 1 is the regex, other columns are
   optinal user defined contents.
   The regex is advanced regex: http://nark.cc/p/?p=1280
   Lines can be continued by backslash(\), same as which in C/C++ language.

Bin-Meta-File format
   This is a tab separated text file, column description:
   Column 1: Integer regex-id, start with 0
   Column 2: Integer number-of-submaches(including the full match)
   Column 3: Boolean do-capturing, 1 or 0
   Column 4: Boolean is-one-pass, 1 or 0
   Column 5: String  copy of full Input-Regex-File line
)EOS"
		, prog);
}

template<class MyDFA>
int self_loop_cnt(const MyDFA& dfa, typename MyDFA::state_id_t s) {
	int self_loop = 0;
	typedef typename MyDFA::state_id_t id_t;
	dfa.for_each_dest(s, [&,s](id_t t) { if (s == t) self_loop++; });
	return self_loop;
}

valvec<BaseDFA*> m_eval_stack;
template<class MyDFA>
bool eval_fast(ComposedRegex<MyDFA>* expr) {
	return eval_fast(expr, '%');
}
template<class MyDFA>
bool eval_fast(ComposedRegex<MyDFA>* expr, char parent_op) {
	if (NULL == expr) return true;
	if (expr->is_leaf()) return true;
	if (expr->op != parent_op) {
		size_t oldsize = m_eval_stack.size();
		for(size_t i = 0; i < dim_of(expr->sub); ++i) {
			if (!eval_fast(expr->sub[i], expr->op))
				return false;
		}
		if (!do_eval_fast(expr, oldsize)) {
			return false;
		}
		m_eval_stack.resize(oldsize);
		m_eval_stack.push_back(&expr->dfa);
	}
	else {
		for(size_t i = 0; i < dim_of(expr->sub); ++i)
			if (!eval_fast(expr->sub[i], expr->op))
				return false;
	}
	return true;
}

template<class MyDFA>
bool do_eval_fast(ComposedRegex<MyDFA>* expr, size_t oldsize) {
	size_t top = m_eval_stack.size();
	switch (expr->op) {
	case '|':
		for (size_t i = oldsize; i < top; ++i) {
		}
		/*
		{
			valvec<size_t> roots(j - i + 1, valvec_reserve());
			if (expr.dfa.total_states())
				roots.push_back(size_t(initial_state));
			for (size_t k = i; k < j; ++k) {
				if (children[k].dfa.total_states())
					roots.push_back(expr.dfa.copy_from(children[k].dfa));
			}
			if (roots.size() >= 2) {
				if (!tmpdfa.dfa_union_n(expr.dfa, roots, size_t(-1), one_regex_timeout_us)) {
					ERROR("Regex Composing: dfa_union_n: eval timeout");
					return false;
				}
				expr.dfa.swap(tmpdfa);
			}
			else if (roots.size() == 1 && expr.dfa.total_states() == 0) {
				for (size_t k = i; k < j; ++k) {
					if (children[i].dfa.total_states()) {
						expr.dfa.swap(children[i].dfa);
						break;
					}
				}
			}
		}
		*/
		break;
	case '&':
/*
		if (j - i == 1) {
		//	ERROR("dfa_intersection: dfa.states=%zd sub[%zd].states=%zd",
		//		expr.dfa.total_states(), i, children[i].dfa.total_states());
			if (expr.dfa.total_states() && children[i].dfa.total_states()) {
				tmpdfa.dfa_intersection(expr.dfa, children[i].dfa);
				expr.dfa.swap(tmpdfa);
			}
			else {
				expr.dfa.resize_states(0);
			}
		//	ERROR("dfa_intersection: result.states=%zd", expr.dfa.total_states());
		}
		else {
		//	ERROR("dfa_intersection: dfa.states=%zd sub[%zd].states=%zd j=%zd",
		//		expr.dfa.total_states(), i, children[i].dfa.total_states(), j);
			valvec<size_t> roots(j - i + 1, valvec_reserve());
			if (expr.dfa.total_states())
				roots.push_back(size_t(initial_state));
			for (size_t k = i; k < j; ++k) {
				if (children[k].dfa.total_states())
					roots[i-k+1] = expr.dfa.copy_from(children[k].dfa);
			}
			if (roots.size() == j - i + 1) {
				if (!tmpdfa.dfa_intersection_n(expr.dfa, roots, size_t(-1), one_regex_timeout_us)) {
					ERROR("Regex Composing: dfa_intersection_n: eval timeout");
					return false;
				}
				expr.dfa.swap(tmpdfa);
			}
			else {
				expr.dfa.resize_states(0);
			}
		}
		*/
		break;
	}
	return true;
}

template<class MyDFA>
bool kleene(MyDFA* res, const MyDFA& dfa, char op) {
	assert('*' == op || '+' == op);
	if (dfa.total_states() == 0) {
		res->resize_states(0);
	} else {
		MyDFA tmp(dfa);
		NFA_BaseOnDFA<MyDFA> nfa(&tmp);
		for (size_t k = 0; k < tmp.total_states(); ++k) {
			if (!tmp.is_free(k) && tmp.is_term(k))
				nfa.push_epsilon(k, initial_state);
		}
		if (!nfa.make_dfa(res, size_t(-1), one_regex_timeout_us)) {
			ERROR("Regex Composing: kleene: eval timeout");
			return false;
		}
		if ('*' == op) {
			res->set_term_bit(initial_state);
		}
		res->graph_dfa_minimize();
	}
	return true;
}

template<class MyDFA>
bool eval_slow(ComposedRegex<MyDFA>* expr) {
	if (NULL == expr) return true;
	if (expr->is_leaf()) return true;
	if (expr->is_evaled) return true;
	for(size_t i = 0; i < dim_of(expr->sub); ++i) {
		if (!eval_slow(expr->sub[i].get())) {
			return false;
		}
	}
	MyDFA* dfa0 = expr->sub[0] ? &expr->sub[0]->dfa : NULL;
	MyDFA* dfa1 = expr->sub[1] ? &expr->sub[1]->dfa : NULL;
//	ERROR("eval: op=%c", expr->op);
	switch (expr->op) {
	default:
		ERROR("eval: Unknown op: %c", expr->op);
		return false;
	case '|':
		if (dfa0->total_states() && dfa1->total_states()) {
			expr->dfa.dfa_union(*dfa0, *dfa1);
			expr->dfa.graph_dfa_minimize();
		}
		else if (dfa0->total_states()) {
			expr->dfa = *dfa0;
		}
		else if (dfa1->total_states()) {
			expr->dfa = *dfa1;
		}
		else expr->dfa.resize_states(0);
		break;
	case '&':
		if (dfa0->total_states() && dfa1->total_states()) {
			expr->dfa.dfa_intersection(*dfa0, *dfa1);
			if (expr->dfa.total_states())
				expr->dfa.graph_dfa_minimize();
		}
		else expr->dfa.resize_states(0);
		break;
	case '!': // difference
		if (dfa0->total_states() && dfa1->total_states()) {
			expr->dfa.dfa_difference(*dfa0, *dfa1);
			if (expr->dfa.total_states())
				expr->dfa.graph_dfa_minimize();
		}
		else if (dfa0->total_states()) {
			expr->dfa = *dfa0;
		}
		else expr->dfa.resize_states(0);
		break;
	case ':': // non-greedy concat
		if (dfa0->total_states() && dfa1->total_states()) {
			// NonGreedyConCat(A#, B) = GreedyConCat((A# &! (A*BA*)+), B)
			// where '#' is any 'Repeat(min=0,max=...)' qualifier
			// MyDFA& A = expr->sub[0]->sub[0].dfa
			MyDFA& B = *dfa1;
			MyDFA  A_star;
			switch (expr->sub[0]->op) {
			default: // A = expr->sub[0]->sub[0].dfa
				if (!kleene(&A_star, expr->sub[0]->sub[0]->dfa, '*'))
					return false;
				break;
			case '*':
				A_star = expr->sub[0]->dfa;
				break;
			}
			MyDFA tmp1(A_star), tmp2;
			NFA_BaseOnDFA<MyDFA> nfa(&tmp1);
			nfa.concat(initial_state, nfa.dfa->fast_copy(B));
			if (!nfa.make_dfa(&tmp2, size_t(-1), one_regex_timeout_us)) {
				fstring code = subcode(expr);
				ERROR("Regex Composing: eval timeout: NonGreedy1(.*, %.*s)",
						code.ilen(), code.data());
				return false;
			}
			tmp1.swap(tmp2); // now tmp1 = A*B
			nfa.erase_all();
			nfa.concat(initial_state, tmp1.fast_copy(A_star));
			if (!nfa.make_dfa(&tmp2, size_t(-1), one_regex_timeout_us)) {
				fstring code = subcode(expr);
				ERROR("Regex Composing: eval timeout: NonGreedy2(%.*s, .*)",
						code.ilen(), code.data());
				return false;
			}
			// now tmp2 = A*BA*
			if (!kleene(&tmp1, tmp2, '+'))
			   	return false;
			tmp2 = std::move(tmp1); // tmp2 = (A*BA*)+
			tmp1.dfa_difference(*dfa0, tmp2); // tmp1 = A# &! (A*BA*)+
			if (tmp1.total_states() == 0) {
				expr->dfa = B;
			} else {
				nfa.erase_all();
				nfa.concat(initial_state, nfa.dfa->fast_copy(B));
				if (!nfa.make_dfa(&expr->dfa, size_t(-1), one_regex_timeout_us)) {
					fstring code = subcode(expr);
					ERROR("Regex Composing: eval timeout: NonGreedy3(%.*s)",
							code.ilen(), code.data());
					return false;
				}
				assert(expr->dfa.total_states() > 0);
				expr->dfa.graph_dfa_minimize();
			}
		}
		else {
			fstring code = subcode(expr);
			ERROR("Regex Composing: operand dfa is empty: NonGreedy(%.*s, .*)",
					code.ilen(), code.data());
			return false;
		}
		break;
	case '.': // concat
		if (dfa0->total_states() && dfa1->total_states()) {
			MyDFA tmp(*dfa0);
			NFA_BaseOnDFA<MyDFA> nfa(&tmp);
			nfa.concat(initial_state, nfa.dfa->fast_copy(*dfa1));
			if (!nfa.make_dfa(&expr->dfa, size_t(-1), one_regex_timeout_us)) {
				fstring code = subcode(expr);
				ERROR("Regex Composing: eval timeout: concat(%.*s)",
						code.ilen(), code.data());
				return false;
			}
			expr->dfa.graph_dfa_minimize();
		}
		else expr->dfa.resize_states(0);
		break;
	case '?':
	RepeatAsk:
		assert(NULL == dfa1);
		if (dfa0->total_states()) {
			expr->dfa = *dfa0;
			expr->dfa.set_term_bit(initial_state);
		}
		break;
	case '*':
	case '+':
	RepeatStarOrPlus:
		assert(NULL == dfa1);
		if (!kleene(&expr->dfa, *dfa0, expr->op))
			return false;
		break;
	case '{': // {min,max}
		assert(NULL == dfa1);
		if (dfa0->total_states() == 0) {
			expr->dfa.resize_states(0);
			break;
		}
		if (0 == expr->rng_min && 1 == expr->rng_max) {
			goto RepeatAsk;
		}
		if (0 == expr->rng_min && -1 == expr->rng_max) {
			expr->op = '*';
			goto RepeatStarOrPlus;
		}
		if (1 == expr->rng_min && -1 == expr->rng_max) {
			expr->op = '+';
			goto RepeatStarOrPlus;
		}
		else {
			MyDFA tmp = *dfa0;
			NFA_BaseOnDFA<MyDFA> nfa(&tmp);
			long rng_min = expr->rng_min;
			long rng_max = expr->rng_max;
			uint32_t root1 = initial_state;
			for (long k = 1; k < rng_min; ++k) {
				uint32_t root2 = tmp.fast_copy(root1);
				nfa.concat(root1, root2);
				root1 = root2;
			}
			if (rng_min < rng_max) {
				valvec<uint32_t> finalstates;
				if (0 == rng_min) {
					tmp.set_term_bit(initial_state);
					rng_min++;
				}
				for (long k = rng_min; k < rng_max; ++k) {
					tmp.get_final_states(root1, &finalstates);
					uint32_t root2 = tmp.fast_copy(root1);
					for (size_t f = 0; f < finalstates.size(); ++f)
						nfa.push_epsilon(finalstates[f], root2);
					root1 = root2;
				}
			}
			else if (-1 == rng_max) {
				valvec<uint32_t> finalstates;
				tmp.get_final_states(root1, &finalstates);
				for (size_t f = 0; f < finalstates.size(); ++f)
					nfa.push_epsilon(finalstates[f], root1);
			}
			if (!nfa.make_dfa(&expr->dfa, size_t(-1), one_regex_timeout_us)) {
				ERROR("Regex Composing: repeat{%ld,%ld}: eval timeout", rng_min, rng_max);
				return false;
			}
		}
		break;
	}
	expr->is_evaled = true;
	return true;
}

template<class MyDFA>
ComposedRegex<MyDFA>* rewrite_expr(ComposedRegex<MyDFA>* expr) {
	if (NULL == expr) return NULL;
	for (auto& x : expr->sub) x = rewrite_expr(x.get());
	switch (expr->op) {
	default:
		break;
	case '?':
		expr->rng_min = 0;
		expr->rng_max = 1;
		break;
	case '*':
		expr->rng_min = 0;
		break;
	case '+':
		expr->rng_min = 1;
		break;
	case ':': {
		ComposedRegex<MyDFA>* repeat = expr->sub[0].get();
		assert(repeat);
		assert(repeat->sub[0]);
		assert(!repeat->sub[1]);
		switch (repeat->op) {
		default:
			break;
		case '+': // rewrite X+:Y to X(X* : Y)
			repeat->op = '*';
			repeat->rng_min = 0;
			return ComposeTwoSub('.', repeat->sub[0].get(), expr);
		case '{':
			long min = repeat->rng_min;
			long max = repeat->rng_max;
			if (min == max) {
				assert(min > 0);
				expr->op = '.'; // rewrite NonGreedyConCat to greedy
			}
			else if (min > 0) {
				ComposedRegex<MyDFA>* sub0(repeat->clone());
				sub0->rng_max = min;
				if (max < 0) { // rewrite X{min,}:Y to X{min}(X* : Y)
					assert(-1 == max);
					repeat->op = '*';
				}
				else { // rewrite X{min,max}:Y to X{min}(X{0,max-min} : Y)
					assert(min < max);
					repeat->rng_max = max - min;
				}
				repeat->rng_min = 0;
				return ComposeTwoSub('.', sub0, expr);
			}
			break;
		}
		break; }
	}
	return expr;
}

template<class MyDFA>
void print_parse_tree(const ComposedRegex<MyDFA>* expr, const char* suffix) {
	char fname[128];
	int regex_id = (int)roots1.size();
	sprintf(fname, "parse_tree-%03d-%s.txt", regex_id, suffix);
	terark::Auto_fclose fp(fopen(fname, "w"));
	if (fp) {
		fprintf(fp, "%.*s\n\n", m_code.ilen(), m_code.data());
		print_parse_tree_loop(fp, 0, expr);
	}
	else {
		ERROR("print_parse_tree: fopen(%s) = %s", fname, strerror(errno));
	}
}

template<class ExprPtr>
fstring subcode(const ExprPtr& expr) {
	auto beg = m_code.data();
	return fstring(beg + expr->pos_beg, beg + expr->pos_end);
}
void print_indent(FILE* fp, size_t depth) {
	for (size_t i = 0; i < depth; ++i) fprintf(fp, "  ");
}
template<class MyDFA>
void print_parse_tree_loop(FILE* fp, size_t depth, const ComposedRegex<MyDFA>* expr) {
#define INDENT_printf(fp, depth, ...) \
	print_indent(fp, depth), \
	fprintf(fp, __VA_ARGS__)
	if (NULL == expr) return;
	fstring code = subcode(expr);
	switch (expr->op) {
	default:
		INDENT_printf(fp, depth, "op=%c : %.*s\n", expr->op, code.ilen(), code.data());
		break;
	case '{':
		INDENT_printf(fp, depth, "op={%ld,%ld} : %.*s\n",
			   	expr->rng_min, expr->rng_max, code.ilen(), code.data());
		break;
	}
	for (size_t i = 0; i < dim_of(expr->sub); ++i) {
		print_parse_tree_loop(fp, depth + 1, expr->sub[i].get());
	}
}

fstring m_code;
size_t  m_pos;

/**
 * Union  := Inter { '||' Union }
 * Inter  := ConCat { '&&' ConCat | '&!' ConCat }
 * ConCat := Repeat { Repeat }
 * Repeat := Atom [ ( '?' | '*' | '+' | Range ) [ ':' Atom ] ]
 * Atom   := '{{' Regex '}}' | '(' Union ')'
 * Range  := '{' Min [ ',' Max ] | ',' Max '}'
 */
template<class MyDFA>
ComposedRegex<MyDFA>* parse() {
	sub_regex_idx = 0;
	if ('*' == m_code[0]) {
		m_pos = 1; force_capture = true;
	} else {
		m_pos = 0; force_capture = false;
	}
	const char* open1 = m_code.strstr(m_pos, "{{");
	if (open1) {
		const char* open0 = open1;
		while (open0 > m_code.begin() && open0[-1] == '\\') --open0;
		size_t n_back_slash = open1 - open0;
		if (n_back_slash && n_back_slash % 2 == 0) {
			// non-zero even n_back_slash implies the first '{' of '{{' will
			// be treated as an non-meta char in regex, this is valid regex
			// syntax, but is very likely to be an error
			ERROR("Regex Composing: col=%zd: %zd escape(\\) chars before '{{'",
					m_pos+1, n_back_slash);
			return NULL;
		}
		if (0 == n_back_slash) {
			is_composing_regex = true;
			ComposedRegexPtr<MyDFA> expr(parse_union<MyDFA>());
			if (expr) {
				size_t end = next_non_space_pos(m_code, m_pos);
				if (end < m_code.size()) {
					ERROR("Regex Composing: col=%zd: premature EOF", m_pos+1);
					return NULL;
				}
			}
			return expr.release();
		}
		// n_back_slash % 2 == 1 implies the last (\) is for escaping '{'
		assert(n_back_slash % 2 == 1);
	}
	ComposedRegexPtr<MyDFA> simple(new ComposedRegex<MyDFA>('#'));
	is_composing_regex = false;
	if (parse_regex(m_code.substr(m_pos), simple->dfa)) {
		return simple.release();
	}
	return NULL;
}

size_t next_non_space_pos(fstring str, size_t pos) {
	while (pos < str.size() && isspace(str[pos])) ++pos;
	return pos;
}

template<class CharOrFstr>
bool skip_space_match_aux(CharOrFstr token) {
   	return m_code.matchAt(m_pos, token);
}
template<class CharOrFstr, class... Tokens>
bool skip_space_match_aux(CharOrFstr head, Tokens... tail) {
	if (m_code.matchAt(m_pos, head))
		return true;
	return skip_space_match_aux(tail...);
}
template<class... Tokens>
bool skip_space_match(Tokens... tokens) {
	m_pos = next_non_space_pos(m_code, m_pos);
	if (m_pos == m_code.size())
		return false;
	return skip_space_match_aux(tokens...);
}

template<class MyDFA>
ComposedRegex<MyDFA>*
ComposeTwoSub(char op, ComposedRegex<MyDFA>* sub0, ComposedRegex<MyDFA>* sub1) {
	ComposedRegex<MyDFA>* expr(new ComposedRegex<MyDFA>(op));
	expr->pos_beg = sub0->pos_beg;
	expr->pos_end = sub1->pos_end;
	expr->sub[0] = sub0;
	expr->sub[1] = sub1;
	return expr;
}
template<class MyDFA>
ComposedRegex<MyDFA>*
ComposeTwoSub(char op, ComposedRegexPtr<MyDFA>& sub0, ComposedRegexPtr<MyDFA>& sub1) {
	ComposedRegex<MyDFA>* expr(new ComposedRegex<MyDFA>(op));
	expr->pos_beg = sub0->pos_beg;
	expr->pos_end = sub1->pos_end;
	expr->sub[0] = sub0.release();
	expr->sub[1] = sub1.release();
	return expr;
}

template<class MyDFA>
ComposedRegex<MyDFA>* parse_union() {
	ComposedRegexPtr<MyDFA> sub0(parse_inter<MyDFA>());
	if (!sub0)
		return NULL;
	while (skip_space_match("||")) {
		m_pos += 2;
		ComposedRegexPtr<MyDFA> sub1(parse_inter<MyDFA>());
		if (!sub1)
			return NULL;
		sub0.reset(ComposeTwoSub('|', sub0, sub1));
	}
	return sub0.release();
}

template<class MyDFA>
ComposedRegex<MyDFA>* parse_inter() {
	ComposedRegexPtr<MyDFA> sub0(parse_concat<MyDFA>());
	if (!sub0)
		return NULL;
	while (skip_space_match("&&", "&!")) {
		char op = m_code[m_pos+1]; // & or !
		m_pos += 2;
		ComposedRegexPtr<MyDFA> sub1(parse_concat<MyDFA>());
		if (!sub1)
			return NULL;
		sub0.reset(ComposeTwoSub(op, sub0, sub1));
	}
	return sub0.release();
}

template<class MyDFA>
ComposedRegex<MyDFA>* parse_concat() {
	ComposedRegexPtr<MyDFA> sub0(parse_repeat<MyDFA>());
   	if (!sub0)
		return NULL;
	while (skip_space_match("{{", '(')) {
		ComposedRegexPtr<MyDFA> sub1(parse_repeat<MyDFA>());
		if (!sub1)
			return NULL;
		sub0.reset(ComposeTwoSub('.', sub0, sub1));
	}
	return sub0.release();
}

template<class MyDFA>
ComposedRegex<MyDFA>* parse_repeat() {
	ComposedRegexPtr<MyDFA> sub0(parse_atom<MyDFA>());
   	if (!sub0)
		return NULL;
	m_pos = next_non_space_pos(m_code, m_pos);
	if (m_code.size() == m_pos) {
		return sub0.release();
	}
	ComposedRegexPtr<MyDFA> expr;
	char op = m_code[m_pos];
	switch (op) {
	default:
		break;
	case '?':
	case '*':
	case '+':
		expr.reset(new ComposedRegex<MyDFA>(op));
		expr->pos_beg = sub0->pos_beg;
		expr->pos_end = sub0->pos_end + 1;
		expr->sub[0] = sub0.release();
		m_pos++;
		goto MaybeNonGreedyConCat;
	case '{':
		if (m_pos+1 == m_code.size()) {
			ERROR("Regex Composing: col=%zd: Range: missing '}'", m_pos+1);
			return NULL;
		}
		if (m_code[m_pos+1] != '{') { // not {{...}}, try to parse range
			long rng_min, rng_max;
			if (!parse_range(rng_min, rng_max)) {
				return NULL;
			}
			expr.reset(new ComposedRegex<MyDFA>('{'));
			expr->rng_min = rng_min;
			expr->rng_max = rng_max;
			expr->pos_beg = sub0->pos_beg;
			expr->pos_end = m_pos;
			expr->sub[0] = sub0.release();
			goto MaybeNonGreedyConCat;
		}
		break;
	}
	return sub0.release();
MaybeNonGreedyConCat:
	if (skip_space_match(':')) {
		m_pos += 1;
		ComposedRegexPtr<MyDFA> suff(parse_atom<MyDFA>());
		if (!suff)
		   	return NULL;
		expr.reset(ComposeTwoSub(':', expr, suff));
	}
	return expr.release();
}

bool parse_range(long& rng_min, long& rng_max) {
	const char* beg = m_code.data() + m_pos + 1;
	const char* end = std::find(beg, m_code.end(), '}');
	if (m_code.end() == end) {
		ERROR("Regex Composing: col=%zd: Range: missing '}'", m_pos+1);
		return false;
	}
	rng_min = strtol(beg, (char**)&end, 10);
	if (end == beg && beg[0] != ',') { // {ERROR //}
		ERROR("Regex Composing: col=%zd: Range error", m_pos);
		return false;
	}
	if (*end == '}') { // {min}
		if (rng_min <= 0) {
			ERROR("Regex Composing: col=%zd: Range error: {min} must > 0", m_pos+1);
			return false;
		}
		rng_max = rng_min;
	}
	else if (*end == ',') {
		if (end == beg) { // {,???}
			end = beg = end + 1;
			rng_max = strtol(beg, (char**)&end, 10);
			if (*end == '}') {
				if (end == beg) { // {,}
					ERROR("Regex Composing: col=%zd: Range error", m_pos+1);
					return false;
				}
			}
			else {
				ERROR("Regex Composing: col=%zd: Range error", m_pos+1);
				return false;
			}
		}
		else { // {min,??? //}
			assert(end > beg);
			end = beg = end + 1;
			rng_max = strtol(beg, (char**)&end, 10);
			if (*end == '}') {
				if (beg == end) { // {min,}
					rng_max = -1;
					goto Check_rng_min;
				}
			}
			else { // {min,ERROR //}
				ERROR("Regex Composing: col=%zd: Range upper bound error: %c",
						beg-m_code.data()+1, *end);
				return false;
			}
		}
		if (rng_max <= 0) {
			ERROR("Regex Composing: col=%zd: Range error: max must > 0", m_pos);
			return false;
		}
		if (rng_max < rng_min) {
			ERROR("Regex Composing: col=%zd: Range error: max must >= min", m_pos);
			return false;
		}
	Check_rng_min:
		if (rng_min < 0) {
			ERROR("Regex Composing: col=%zd: Range error: min must >= 0", m_pos);
			return false;
		}
	}
	else {
		return false;
	}
	m_pos = end - m_code.data() + 1;
	return true;
}

template<class MyDFA>
ComposedRegex<MyDFA>* parse_atom() {
	if (skip_space_match("{{")) {
		const char* end = strstr(m_code.p + m_pos + 2, "}}");
		if (NULL == end) {
			ERROR("Regex Composing: col=%zd: not found ending '}}' after here", m_pos+1);
			return NULL;
		}
		while (end < m_code.end() && end[2] == '}') ++end;
		ComposedRegexPtr<MyDFA> expr(new ComposedRegex<MyDFA>('#'));
		expr->pos_beg = m_pos;
		fstring atom(m_code.data() + m_pos + 2, end);
		if (!parse_regex(atom, expr->dfa)) {
			return NULL;
		}
		expr->pos_end = m_pos = end + 2 - m_code.begin();
		return expr.release();
	}
	else if (m_code.matchAt(m_pos, "(")) {
		m_pos += 1;
		ComposedRegexPtr<MyDFA> expr(parse_union<MyDFA>());
		if (!expr)
			return NULL;
		if (!skip_space_match(')')) {
			ERROR("Regex Composing: col=%zd: missing ending ')'", m_pos+1);
			return NULL;
		}
		m_pos += 1;
		return expr.release();
	}
	else {
		ERROR("Regex Composing: col=%zd: Invalid seq=%.4s", m_pos+1, m_code.p+m_pos);
		return NULL;
	}
}

template<class MyDFA>
bool parse_regex(fstring regex1, MyDFA& mdfa) {
	StringPiece regex(regex1.p, regex1.n);
	RegexpStatus err;
	int flags2 = flags;
	if (!with_submatch_capture)
		flags2 |= Regexp::NeverCapture;
	if (ignore_case)
		flags2 |= Regexp::FoldCase;
	Regexp* re = Regexp::Parse(regex, Regexp::ParseFlags(flags2), &err);
	if (re == NULL) {
		ERROR("Regex Syntax Error: %s", err.Text().c_str());
		return false;
	}
	size_t max_mem = 128*1024*1024; // 128M
	std::unique_ptr<Prog> prog(re->CompileToProg(max_mem));
	if (!prog) {
		ERROR("RE2: CompileToProg");
		re->Decref();
		return false;
	}
	RE2_VM_NFA vmnfa(prog.get());
	int32_t regex_id = int32_t(roots1.size());
	if (is_composing_regex) {
		is_onepass = false;
		do_capture = false;
		vmnfa.m_exclude_char = -1;
	} else {
		is_onepass = prog->IsOnePass();
		do_capture = 2 == with_submatch_capture ||
					(1 == with_submatch_capture && (is_onepass || force_capture));
		vmnfa.m_add_dot_star = 1 == add_dot_star;
		vmnfa.m_exclude_char = SUB_MATCH_DELIM;
		vmnfa.m_do_capture   = do_capture;
		vmnfa.m_use_bin_val  = true; // now is always binary mode
		vmnfa.m_val_prefix.assign(reinterpret_cast<char*>(&regex_id), 4);
		vmnfa.compile();
	}
	if (!be_quiet) {
		fprintf(stderr, "line:%ld: make_dfa[%d]: nfa-states=%zd ... "
				, lineno, sub_regex_idx, vmnfa.total_states());
		fflush(stderr);
	}
	MyDFA dfa;
	if (!vmnfa.make_dfa(&dfa, size_t(-1), one_regex_timeout_us)) {
		if (be_quiet)
			ERROR("make_dfa[%d]: nfa-states=%zd ... Failed: Timeout",
				   	sub_regex_idx, vmnfa.total_states());
		else
			HIGH_LIGHT("Failed: Timeout");
		return false;
	}
	dfa.graph_dfa_minimize(mdfa);
	if (!be_quiet) {
		fprintf(stderr, "successed, dfa-states[raw=%zd min=%zd]\n"
				, dfa.total_states(), mdfa.total_states());
	}
	if (write_dot_graph) {
		char buf[64];
		long len = sprintf(buf, "%03d-%03d", regex_id, sub_regex_idx);
		fstring idstr(buf, len);
		vmnfa.write_dot_file(("one-" + idstr + "-nfa.dot").c_str());
		dfa  .write_dot_file(("one-" + idstr + "-dfa.dot").c_str());
		mdfa .write_dot_file(("one-" + idstr + "-min.dot").c_str());
	}
	n_submatch = is_composing_regex ? 1 : 1 + re->NumCaptures();
	re->Decref();
	sub_regex_idx++;
	return true;
}

template<class MyDFA>
int build() {
	fprintf(stderr, "Function: %s\n", BOOST_CURRENT_FUNCTION);
	terark::profiling pf;
	MyDFA alldfa;
	terark::LineBuf line;
	std::string multiline;
	valvec<fstring> F;
	long next_lineno = 1;
	long nTotal = 0;
	long nEmpty = 0;
	long failed = 0;
	bool is_eof = false;
	if (NULL == finput) {
		HIGH_LIGHT("Reading from stdin...");
	}
	auto t0 = pf.now();
	while (!is_eof) {
		lineno = next_lineno;
		multiline.resize(0);
		while (!(is_eof = line.getline(finput.self_or(stdin)) < 0)) {
			next_lineno++;
			line.chomp();
			if (line.empty()) break;
			if ('\\' == line.end()[-1]) {
				multiline.append(line.begin(), line.size()-1);
			} else {
				long pos = line.size() - 1;
				while (pos >= 0 && isspace(line[pos] & 255)) --pos;
				if (pos >= 0 && '\\' == line[pos]) {
					WARN("spaces after line continue char(\\)");
				}
				multiline.append(line.begin(), line.size());
			   	break;
			}
		}
		if (multiline.empty() && is_eof) {
			break;
		}
		nTotal++;
		if ('#' == multiline[0]) {
			// Comment
			continue;
		}
		fstring(multiline).split('\t', &F);
		if (F.empty()) {
			WARN("Empty line, Skiped");
			nEmpty++;
			continue;
		}
		m_code = F[0];
		ComposedRegexPtr<MyDFA> expr(parse<MyDFA>());
		if (!expr) {
			failed++;
			continue;
		}
		if (is_composing_regex) {
			if (do_print_parse_tree) print_parse_tree(expr.get(), "orig");
			ComposedRegex<MyDFA>* tmp = rewrite_expr(&*expr);
			expr.release();
			expr.reset(tmp);
			if (do_print_parse_tree) print_parse_tree(tmp, "rewrite");
			if (!eval_slow(&*expr)) {
				failed++;
				continue;
			}
		}
		if (expr->dfa.total_states() == 0) {
			ERROR("Regex Composing Result is empty");
			failed++;
			continue;
		}
	#if 0
		if (!be_quiet) {
			fprintf(stderr, "dfa.states=%zd\n", expr->dfa.total_states());
			expr->dfa.write_dot_file("expr.dfa.dot");
		}
	#endif
		int self_loop = self_loop_cnt(expr->dfa, initial_state);
	//	fprintf(stderr, "line:%ld: self_loop=%d\n", lineno, self_loop);
		if (self_loop > 64) {
			// do not warning for dynamic_dfa_matching
			if (!add_dot_star && !dynamic_dfa_matching) {
				WARN("possible .* at regex beginning");
			}
		}
		fprintf(bin_meta_fp, "%d\t%d\t%d\t%d\t%s\n",
			(int)roots1.size(), n_submatch, do_capture, is_onepass, multiline.c_str());
		if (is_composing_regex) {
			int32_t regex_id = (int32_t)roots1.size();
			fstring regex_id_data(reinterpret_cast<char*>(&regex_id), 4);
			if (write_dot_graph) {
				char fname[64];
				sprintf(fname, "one-%03d-com-no-id.dot", regex_id);
				expr->dfa.write_dot_file(fname);
			}
			expr->dfa.append_delim_value(FULL_MATCH_DELIM, regex_id_data);
			if (write_dot_graph) {
				char fname[64];
				sprintf(fname, "one-%03d-com-yy-id.dot", regex_id);
				expr->dfa.write_dot_file(fname);
			}
		}
		if (enumConstBaseFileName) {
			if (F.size() < 2) {
				ERROR("specified enumConstBaseFileName but has no colum 2 for const name, skipped\n");
			}
			else {
				fstring constName = F[1];
				enumConstMap[constName].push_back((int)roots1.size());
			}
		}
		roots1.push_back(alldfa.fast_copy(expr->dfa));
	}
	if (roots1.empty()) {
		fprintf(stderr, "error: no any valid regex\n");
		return 2;
	}
	if (enumConstBaseFileName) {
		std::string cppFile = std::string(enumConstBaseFileName) + ".cpp";
		std::string hppFile = std::string(enumConstBaseFileName) + ".hpp";
		Auto_fclose cppFp(fopen(cppFile.c_str(), "w"));
		Auto_fclose hppFp(fopen(hppFile.c_str(), "w"));
		if (!hppFp) {
			FATAL("fopen('%s', 'w') = %s", hppFile.c_str(), strerror(errno));
		}
		if (!cppFp) {
			FATAL("fopen('%s', 'w') = %s", cppFile.c_str(), strerror(errno));
		}
		else {
			std::string guardMacro = enumConstBaseFileName;
			std::replace_if(guardMacro.begin(), guardMacro.end(), [](byte_t c){return !isalnum(c);}, '_');
			fprintf(hppFp, "#ifndef __enum_const_%s_hpp__\n", enumConstBaseFileName);
			fprintf(hppFp, "#define __enum_const_%s_hpp__\n", enumConstBaseFileName);
			fprintf(hppFp, "\n");
			fprintf(hppFp, "namespace %s {\n", enumConstNamespace);
			fprintf(hppFp, "\n");
			fprintf(hppFp, "  enum Enum {\n");
			fprintf(hppFp, "    UnamedRegexID = -1,\n");
			valvec<int> mapIdToName(roots1.size(), -1);
			for (size_t i = 0; i < enumConstMap.end_i(); ++i) {
				auto& idvec = enumConstMap.val(i);
				for (size_t j = 0; j < idvec.size(); ++j) {
					mapIdToName[idvec[j]] = i;
				}
				fprintf(hppFp, "    %-30s = %8d,\n", enumConstMap.key_c_str(i), int(i));
			}
			fprintf(hppFp, "  };\n");
			fprintf(hppFp, "\n");
			fprintf(hppFp, "  extern const enum Enum EnumOfRegexID[];\n");
			fprintf(hppFp, "\n");
			fprintf(hppFp, "} /""/ %s\n", enumConstNamespace);
			fprintf(hppFp, "\n");
			fprintf(hppFp, "#endif /""/ __enum_const_%s_hpp__\n", enumConstBaseFileName);

			fprintf(cppFp, "#include \"%s\"\n", hppFile.c_str());
			fprintf(cppFp, "\n");
			fprintf(cppFp, "namespace %s {\n", enumConstNamespace);
			fprintf(cppFp, "  const enum Enum EnumOfRegexID[] = {\n");
			for (size_t i = 0; i < mapIdToName.size(); ++i) {
				fprintf(cppFp, "    "); // indent
				if (-1 == mapIdToName[i])
					fprintf(cppFp, "%s,\n", "UnamedRegexID");
				else
					fprintf(cppFp, "%s,\n", enumConstMap.key_c_str(mapIdToName[i]));
			}
			fprintf(cppFp, "  };\n");
			fprintf(cppFp, "} /""/ %s\n", enumConstNamespace);
		}
	}
	MyDFA unioned_dfa;
	MyDFA MinDFA;
	auto t1 = pf.now();
	fprintf(stderr, "time: build all sub DFA: %f's\n", pf.sf(t0,t1));
	size_t ps_size = 0;
	if (!dynamic_dfa_matching || roots1.size() == 1) {
		if (all_regex_timeout_seconds) {
			fprintf(stderr, "start union all sub DFA with timeout=%zd'seconds\n", all_regex_timeout_seconds);
		}
	   	else if (roots1.size() > 1) {
			fprintf(stderr, "start union all sub DFA without timeout\n");
			fprintf(stderr, "   Patient... this maybe very slow and run out of memory\n");
			fprintf(stderr, "   If failed, try to add option -D to build a dynamic matching FSA\n");
		}
		size_t timeout_us = all_regex_timeout_seconds * 1000000;
		ps_size = unioned_dfa.dfa_union_n(alldfa, roots1, size_t(-1), timeout_us);
		if (0 == ps_size) { // make dfa failed
			unioned_dfa.erase_all();
			fprintf(stderr, "FATAL: In dfa_union_n, failed because power set explosion\n");
			fprintf(stderr, "   Exceeded timeout=%zd'seconds by option -T\n", all_regex_timeout_seconds);
			fprintf(stderr, "   Try to add option -D to build a dynamic matching FSA\n");
			return 1;
		}
	}
	dynaRoots1.resize_no_init(roots1.size()+1);
	dynaRoots2.resize_no_init(roots1.size()+1);
	dynaRoots1[0] = initial_state; // unioned_dfa.initial_state
	roots1.push_back(alldfa.total_states()); // append the guard 'root'
	for(size_t i = 1; i < dynaRoots1.size(); ++i) {
		size_t root = roots1[i-1];
		size_t size = roots1[i] - roots1[i-1];
		dynaRoots1[i] = unioned_dfa.fast_copy(alldfa, root, size);
	}
	if (dynamic_dfa_matching) {
		valvec<DfaPart> atom_roots(roots1.size(), valvec_reserve());
		valvec<DfaPart> cluster_roots;
		for(size_t i = 1; i < dynaRoots1.size(); ++i) {
			size_t root = roots1[i-1];
			size_t size = roots1[i] - roots1[i-1];
			atom_roots.push({root, size});
		}
		MyDFA cluster;
		cluster_dfa(alldfa, atom_roots, cluster, cluster_roots);
		assert(dynaRoots1.size() == roots1.size());
		dynaRoots1.resize_no_init(roots1.size() + cluster_roots.size());
		size_t ustart = unioned_dfa.fast_copy(cluster);
		for(size_t i = 0; i < cluster_roots.size(); ++i) {
			dynaRoots1[roots1.size() + i] = ustart + cluster_roots[i].root;
		}
		for(size_t i = 1; i < dynaRoots1.size(); ++i) {
			auchar_t FakeEpsilonChar = 258;
			unioned_dfa.add_move(dynaRoots1[i-1], dynaRoots1[i], FakeEpsilonChar);
		}
	}
	roots1.pop_back(); // remove the guard 'root'
	auto t2 = pf.now();
	fprintf(stderr, "time: union all sub DFA: %f's\n", pf.sf(t1,t2));
	unioned_dfa.hopcroft_multi_root_dfa(dynaRoots1, MinDFA, &dynaRoots2);
	auto t3 = pf.now();
#if 0
	for (size_t i = 0; i < dynaRoots2.size(); ++i) {
		size_t r1 = dynaRoots1[i];
		size_t r2 = dynaRoots2[i];
		fprintf(stderr, "hopcroft root[%zd]: %zd -> %zd\n", i, r1, r2);
	}
#endif
	fprintf(stderr, "time: minimize unioned_dfa: %f's\n", pf.sf(t2,t3));
	if (2 == add_dot_star && ps_size) {
		// this branch is just for test
		NFA_BaseOnDFA<MyDFA> nfa(&alldfa);
		for (size_t i = 0; i < roots1.size(); ++i) {
			nfa.push_epsilon(initial_state, roots1[i]);
		}
		for (int c = 0; c < 256; ++c) {
			nfa.add_move(initial_state, initial_state, c);
		}
		nfa.make_dfa(&unioned_dfa);
		auto t4 = pf.now();
		fprintf(stderr, "time: DotStarNFA to DFA: %f's\n", pf.sf(t3,t4));
		unioned_dfa.graph_dfa_minimize(MinDFA);
		auto t5 = pf.now();
		fprintf(stderr, "time: minimize DotStarDFA: %f's\n", pf.sf(t4,t5));
	}
	fprintf(stderr, "Original__UnionDFA: states=%zd transitions=%zd mem=%zd | per-state: trans=%.3f mem=%.3f | fragmem=%zd\n"
			, unioned_dfa.total_states()
			, unioned_dfa.total_transitions()
			, unioned_dfa.mem_size()
			, unioned_dfa.total_transitions()/double(unioned_dfa.total_states())
			, unioned_dfa.mem_size()/double(unioned_dfa.total_states())
			, unioned_dfa.frag_size()
			);
	fprintf(stderr, "Minimized_UnionDFA: states=%zd transitions=%zd mem=%zd | per-state: trans=%.3f mem=%.3f | fragmem=%zd\n"
			, MinDFA.total_states()
			, MinDFA.total_transitions()
			, MinDFA.mem_size()
			, MinDFA.total_transitions()/double(MinDFA.total_states())
			, MinDFA.mem_size()/double(MinDFA.total_states())
			, MinDFA.frag_size()
			);
#if 0
	MinDFA.compact();
	fprintf(stderr, "Compacted_UnionDFA: states=%zd transitions=%zd mem=%zd | per-state: trans=%.3f mem=%.3f\n"
			, MinDFA.total_states()
			, MinDFA.total_transitions()
			, MinDFA.mem_size()
			, MinDFA.total_transitions()/double(MinDFA.total_states())
			, MinDFA.mem_size()/double(MinDFA.total_states())
			);
#endif
	report_conflict(MinDFA, conflict_report_file);
	MyDFA pzip;
	if (g_minZpathLen && ps_size)
		pzip.path_zip(dynaRoots2, MinDFA, "DFS");
	else
		pzip.normalize(dynaRoots2, MinDFA, "DFS");
	fprintf(stderr, "%sUnionDFA: states=%zd transitions=%zd mem=%zd | per-state: trans=%.3f mem=%.3f | fragmem=%zd\n"
			, g_minZpathLen && ps_size ? "Pathziped_" : "Normalized"
			, pzip.total_states()
			, pzip.total_transitions()
			, pzip.mem_size()
			, pzip.total_transitions()/double(pzip.total_states())
			, pzip.mem_size()/double(pzip.total_states())
			, pzip.frag_size()
			);
	assert(dynaRoots2.size() == dynaRoots1.size());
	valvec<size_t> i2i(dynaRoots1.size(), valvec_no_init());
	for (size_t i = 0; i < i2i.size(); ++i) i2i[i] = i;
	if (write_dot_graph_unioned) {
		// i2i represent the normalized roots state id
		// all-dfa-11a means one-one...-dfa and all-dfa
		unioned_dfa.multi_root_write_dot_file(dynaRoots1, "all-dfa-11a.dot");
		MinDFA.multi_root_write_dot_file(dynaRoots2, "all-dfa-min.dot");
		pzip.multi_root_write_dot_file(i2i, "all-dfa-zip.dot");
	}
	pzip.set_is_dag(pzip.tpl_is_dag());
	pzip.set_kv_delim(SUB_MATCH_DELIM);
	pzip.set_sigma(REGEX_DFA_SIGMA); // 259
	pzip.m_atom_dfa_num = roots1.size()+1; // include initial_state
	pzip.m_dfa_cluster_num = dynaRoots1.size() - (roots1.size()+1);
	if (out_dfa_file)
		pzip.save_mmap(out_dfa_file);
	if (out_min_file) {
		VirtualMachineDFA smalldfa;
		smalldfa.build_from(pzip, i2i);
		smalldfa.save_mmap(out_min_file);
	}
	fprintf(stderr, "total: %ld  successed: %zd  failed: %ld  empty: %ld\n",
		   	nTotal, roots1.size(), failed, nEmpty);
	return 0;
}

template<class MyDFA>
void report_conflict(const MyDFA& MinDFA, const char* fname) {
	if (NULL == fname)
	   	return;
	terark::FileStream fp(fname, "w");
	fstrvec conflicts;
	valvec<uint32_t> stack;
	febitvec color1(MinDFA.total_states());
	febitvec color2(MinDFA.total_states());
	stack.push_back(initial_state);
	while (!stack.empty()) {
		uint32_t parent = stack.pop_val();
		MinDFA.for_each_move(parent,
		[&](uint32_t child, auchar_t ch) {
			if (color1.is0(child)) {
				color1.set1(child);
				stack.push_back(child);
			}
			if (color2.is0(child) && FULL_MATCH_DELIM == ch) {
				conflicts.erase_all();
				MinDFA.tpl_for_each_word(child, 0,
					[&](size_t,fstring name, size_t) {
						conflicts.push_back(name);
					});
				if (conflicts.size() > 1) {
					fprintf(fp, "DFA state=%u, conflict regex_id:\n", child);
					for (size_t i = 0; i < conflicts.size(); ++i) {
						fstring name = conflicts[i];
						int32_t regex_id = unaligned_load<int32_t>(name.udata());
						fprintf(fp, "\t%d\n", regex_id);
					}
				}
				color2.set1(child);
			}
		});
	}
}

template<class MyDFA>
void cluster_dfa(const MyDFA& srcDFA, const valvec<DfaPart>& srcPart,
					   MyDFA& dstDFA,       valvec<DfaPart>& dstPart)
{
	valvec<DfaPart> stk1(srcPart);
	valvec<DfaPart> stk2;
	valvec<DfaPart> bad;
	MyDFA dfa1 = srcDFA;
	MyDFA dfa2;
	MyDFA smalldfa;
	std::mt19937_64 random;
	auto do_union = [&]() {
		std::shuffle(stk1.begin(), stk1.end(), random);
		while (stk1.size() >= 2) {
			DfaPart d1 = stk1.pop_val();
			DfaPart d2 = stk1.pop_val();
			size_t maxpower = limit_cluster_union_power_size
				? (d1.size + d2.size) * 3
				: size_t(-1);
			if (smalldfa.dfa_union(dfa1, d1.root, d1.size, dfa1, d2.root, d2.size, maxpower)) {
				smalldfa.graph_dfa_minimize();
				size_t root = dfa2.fast_copy(smalldfa);
				stk2.push({root, smalldfa.total_states()});
			}
			else {
				bad.push(d1);
				bad.push(d2);
			}
		}
		assert(stk1.size() <= 1);
		if (stk1.size() == 1)
			bad.push(stk1.pop_val());
	};
	terark::profiling pf;
	size_t max_loop = (size_t)(log2(srcPart.size()+1) + 2.5);
	for (size_t loop = 0; loop < max_loop; ++loop) {
		fprintf(stderr, "clustering: loop=%02zd cluster-num=%08zd states=%09zd"
				, loop, stk1.size(), dfa1.total_states());
		if (stk1.size() == 1) {
			fprintf(stderr, ", good luck, completely unioned!\n");
			goto Done;
		}
		auto t0 = pf.now();
		assert(bad.empty());
		assert(stk2.empty());
		do_union();
		if (stk2.empty()) {
			auto t1 = pf.now();
			fprintf(stderr, ", time=%f's, bad luck! stop.\n", pf.sf(t0, t1));
			stk1.swap(bad);
			goto Done;
		}
		if (bad.size() >= 4) {
			stk1.swap(bad);
			do_union();
		}
		while (!bad.empty()) {
			DfaPart d1 = bad.pop_val();
			d1.root = dfa2.fast_copy(dfa1, d1.root, d1.size);
			stk2.push(d1);
		}
		stk1.swap(stk2); stk2.erase_all();
		dfa1.swap(dfa2); dfa2.erase_all();
		auto t1 = pf.now();
		fprintf(stderr, ", time=%f's\n", pf.sf(t0, t1));
	}
	fprintf(stderr, "clustering: loop=%02zd cluster-num=%08zd states=%09zd, completed!\n"
			, max_loop, stk1.size(), dfa1.total_states());
Done:
	dstDFA.swap(dfa1);
	dstPart.swap(stk1);
}

void check_fname_begin(int opt) {
	if ('-' == optarg[0]) {
		HIGH_LIGHT(
			"ERROR: option: argument of -%c is %s --- it seems like another option!",
			opt, optarg);
		exit(1);
	}
	const char* invalid_beg = "@:=+(){}[],.*!~`'\"&^%";
	if (strchr(invalid_beg, optarg[0])) {
		HIGH_LIGHT(
			"ERROR: invalid option: -%c argument: %s --- can not begin with any of %s",
			opt, optarg, invalid_beg);
		exit(1);
	}
}

int main(int argc, char** argv) {
	int dfa_type = 'd';
	const char* finput_name = NULL;
	for (;;) {
		int opt = getopt(argc, argv, "a::gGhLO:o:t:c:E:s::b:z:DIqT:P");
		switch (opt) {
		default:
		case 'h':
		case '?':
			usage(argv[0]);
			return 3;
		case -1:
			goto GetoptDone;
		case 'c':
			conflict_report_file = optarg;
			check_fname_begin(opt);
			break;
		case 'E':
			enumConstBaseFileName = optarg;
			enumConstNamespace = strchr(enumConstBaseFileName, '@');
			if (enumConstNamespace)
				enumConstNamespace++[0] = '\0';
			else
				enumConstNamespace = enumConstBaseFileName;
			break;
		case 'O':
			check_fname_begin(opt);
			out_dfa_file = optarg;
			break;
		case 'o':
			check_fname_begin(opt);
			out_min_file = optarg;
			break;
		case 'a': // add DotStar in sub dfa
			add_dot_star = optarg ? atoi(optarg) : 1;
			break;
		case 'g':
			write_dot_graph = true;
			break;
		case 'G':
			write_dot_graph_unioned = true;
			break;
		case 'I':
			ignore_case = true;
			break;
		case 'D':
			dynamic_dfa_matching = true;
			break;
		case 'L': // utf8 is default, Latin1 is explicit
			flags |= Regexp::Latin1;
			break;
		case 's':
			if (optarg && strcmp(optarg, "s") == 0)
				with_submatch_capture = 2;
			else
				with_submatch_capture = 1;
			break;
		case 'b':
			check_fname_begin(opt);
			bin_meta_file = optarg;
			break;
		case 't':
			assert(NULL != optarg);
			dfa_type = optarg[0];
			break;
		case 'T':
			// -T one_regex_timeout_milliseconds[:all_regex_timeout_seconds]
			// timeout delim can be  ':'  ','  ';'  ' '
			// all_regex_timeout_seconds is optional
			{
				char* t1_end = NULL;
				one_regex_timeout_us = strtoull(optarg, &t1_end, 10) * 1000;
				if (*t1_end && strchr(",;: ", *t1_end))
					all_regex_timeout_seconds = strtoull(t1_end+1, NULL, 10);
			}
			break;
		case 'z':
			assert(NULL != optarg);
			if (optarg) {
				g_minZpathLen = atoi(optarg);
				g_minZpathLen = std::max(g_minZpathLen, 0);
			}
			break;
		case 'q':
			be_quiet = true;
			break;
		case 'P':
			limit_cluster_union_power_size = false;
			break;
		}
	}
GetoptDone:
	if (optind < argc) {
		finput_name = argv[optind];
	}
	if (dynamic_dfa_matching && !bin_meta_file) {
		HIGH_LIGHT("FATAL: When using option -D, must use -b bin_meta_file also.\n");
		usage(argv[0]);
		return 3;
	}
	if (NULL == out_dfa_file && NULL == out_min_file) {
		HIGH_LIGHT("FATAL: Missing argument -O normal-dfa-file or -o zipped-dfa-file\n");
		usage(argv[0]);
		return 3;
	}
	if (finput_name) {
		finput = fopen(finput_name, "r");
		if (NULL == finput) {
			HIGH_LIGHT("FATAL: fopen(%s, r) = %s\n", finput_name, strerror(errno));
			return 3;
		}
	}
	if (bin_meta_file) {
		bin_meta_fp.open(bin_meta_file, "w");
		if (NULL == bin_meta_fp) {
			HIGH_LIGHT("FATAL: fopen(%s, w) = %s\n", bin_meta_file, strerror(errno));
			return 3;
		}
	}
	else {
		HIGH_LIGHT("FATAL: Missing argument -b bin_meta_file");
		usage(argv[0]);
		return 3;
	}
//	fprintf(stderr, "with_submatch_capture=%d\n", with_submatch_capture);
	int ret;
	if ('d' == dfa_type)
		ret = build<DenseDFA<uint32_t, 320> >();
	else if ('2' == dfa_type)
		ret = build<DenseDFA_V2<uint32_t, 288> >();
	else {
#if defined(TERARK_INST_ALL_DFA_TYPES)
		ret = build<Automata<State32_512> >();
#else
		fprintf(stderr, "invalid -t option\n");
		usage(argv[0]);
		ret = 3;
#endif
	}
	return ret;
}

}; // class MultiRegexBuilder

int main(int argc, char** argv) {
//	printf("sizeof(CharTarget<size_t>)=%zd\n", sizeof(CharTarget<size_t>));
	MultiRegexBuilder builder;
	try {
		int ret = builder.main(argc, argv);
		return ret;
	}
	catch (const std::exception&) {
		return 1;
	}
}


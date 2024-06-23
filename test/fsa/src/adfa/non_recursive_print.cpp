#include <terark/fsa/fsa.hpp>
#include <terark/fsa/automata.hpp>
#include <terark/util/autofree.hpp>
#include <terark/util/linebuf.hpp>
#include <terark/util/profiling.hpp>
#include <getopt.h>

#if defined(__DARWIN_C_ANSI)
	#define malloc_stats() (void)(0)
#else
	#include <malloc.h>
#endif

using namespace terark;

int usage(const char* prog) {
	fprintf(stderr, "usage: %s DFA-File\n", prog);
	return 3;
}

struct NonRecursiveForEachValue {
	struct IncomingEdge {
#if TERARK_WORD_BITS == 64
		size_t state : 55;
		size_t label :  9;
#else
		size_t state;
		size_t label;
#endif
		void set_visited(size_t zplen) {
			label = 256;
			state = zplen;
		}
		bool is_visited() const { return 256 == label; }
	};

	size_t const PAD_LEN = 8;

	MatchContext   ctx;
	valvec<byte_t> value;
	valvec<IncomingEdge> stack;

	NonRecursiveForEachValue();
	~NonRecursiveForEachValue();

	template<class DFA, class OnValue>
	size_t
	run(const DFA* dfa, size_t root, size_t SkipLen, OnValue onValue);
};

NonRecursiveForEachValue::NonRecursiveForEachValue() {
	value.reserve(64);
	value.resize(PAD_LEN-1);
	stack.reserve(1*1024);
}

NonRecursiveForEachValue::~NonRecursiveForEachValue() {
}

template<class DFA, class OnValue>
size_t
NonRecursiveForEachValue::
run(const DFA* dfa, size_t root, size_t SkipLen, OnValue onValue) {
	assert(stack.empty());
	assert(value.size() == PAD_LEN-1);
	value.reserve(1024);
	value.resize(PAD_LEN-1);
	stack.push_back({root, '\0'});
	size_t nth = 0;
	while (!stack.empty()) {
		auto top = stack.back();
		if (top.is_visited()) {
			size_t zplen = top.state; // now top.state is zplen
			assert(value.size() >= 1 + zplen + PAD_LEN-1);
			value.resize(value.size() - (1 + zplen));
			stack.pop_back();
		}
		else {
			assert(top.state < dfa->total_states());
			value.push_back(top.label);
			size_t zplen = 0;
			if (dfa->is_pzip(top.state)) {
				fstring zstr = dfa->get_zpath_data(top.state, &ctx);
				value.append(zstr.begin(), zstr.size());
				zplen = zstr.size();
			}
			if (dfa->is_term(top.state)) {
				value.push_back('\0');
				fstring val(value.data() + PAD_LEN, value.size() - PAD_LEN - 1);
				onValue(nth, val, top.state);
				value.pop_back();
				nth++;
			}
			size_t oldsize = stack.size();
			stack[oldsize-1].set_visited(zplen);
			dfa->for_each_move(top.state, [&](size_t child, auchar_t label) {
				if (DFA::sigma > 256 && label >= 256) {
					// this THROW will be optimized out when DFA::sigma <= 256
					THROW_STD(out_of_range,
						"expect a byte, but got label=%d(0x%X)", label, label);
				}
				this->stack.push_back({child, label});
			});
			std::reverse(stack.begin() + oldsize, stack.end());
		}
	}
	return nth;
}

int main(int argc, char* argv[]) {
	for (;;) {
		int opt = getopt(argc, argv, "");
		switch (opt) {
		default:
			return usage(argv[0]);
		case -1:
			goto GetoptDone;
		}
	}
GetoptDone:
	if (optind >= argc) {
		return usage(argv[0]);
	}

	const char* fname = argv[optind];
	std::unique_ptr<BaseDFA> dfa(BaseDFA::load_from(fname));

	auto printValue = [](size_t nth, fstring val, size_t) {
		*(char*)val.end() = '\n'; // overwrite the terminal '\0'
		fwrite(val.data(), 1, val.size()+1, stdout);
	};
	NonRecursiveForEachValue fev;
	if (auto au = dynamic_cast<const Automata<State4B>*>(dfa.get())) {
		fev.run(au, 0, 0, printValue);
	}
	else if (auto au = dynamic_cast<const Automata<State5B>*>(dfa.get())) {
		fev.run(au, 0, 0, printValue);
	}
	else if (auto au = dynamic_cast<const Automata<State6B>*>(dfa.get())) {
		fev.run(au, 0, 0, printValue);
	}
	else {
		fprintf(stderr, "unknown DFA\n");
	}
	return 0;
}


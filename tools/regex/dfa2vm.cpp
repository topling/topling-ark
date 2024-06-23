#define _CRT_SECURE_NO_WARNINGS
#define _SCL_SECURE_NO_WARNINGS
#include <terark/fsa/dense_dfa.hpp>
#include <terark/fsa/dense_dfa_v2.hpp>
#include <terark/fsa/virtual_machine_dfa.hpp>
#include <terark/fsa/virtual_machine_dfa_builder.hpp>

using namespace terark;

class Main {
public:
	template<class MyDFA>
	valvec<uint32_t> get_roots(const MyDFA* dfa) {
		auchar_t FakeEpsilonChar = 258;
		uint32_t curr = dfa->state_move(initial_state, FakeEpsilonChar);
		valvec<uint32_t> roots;
		roots.push_back(initial_state);
		while (MyDFA::nil_state != curr) {
			roots.push_back(curr);
			curr = dfa->state_move(curr, FakeEpsilonChar);
		}
		return roots;
	}

	template<class MyDFA>
	void convert_to_vm(const MyDFA* dfa, const char* vm_fname) {
		valvec<uint32_t> roots = get_roots(dfa);
		VirtualMachineDFA smalldfa;
		smalldfa.build_from(*dfa, roots);
		smalldfa.save_mmap(vm_fname);
		printf("input.roots.size() = %d\n", (int)roots.size());
		printf("small.roots.size() = %d\n", (int)smalldfa.num_roots());
	}

	int main(int argc, char* argv[]) {
		if (argc < 3) {
			fprintf(stderr, "Usage: %s src_dfa_file dest_vm_file\n", argv[0]);
			return 1;
		}
		std::unique_ptr<BaseDFA> idfa(BaseDFA::load_from(argv[1]));
		if (auto dfa = dynamic_cast<DenseDFA_uint32_320*>(idfa.get())) {
			convert_to_vm(dfa, argv[2]);
			return 0;
		}
		if (auto dfa = dynamic_cast<DenseDFA_V2_uint32_288*>(idfa.get())) {
			convert_to_vm(dfa, argv[2]);
			return 0;
		}
		return 0;
	}
};

int main(int argc, char* argv[]) {
	try {
		return Main().main(argc, argv);
	}
	catch (const std::exception&) {
		return 1;
	}
}


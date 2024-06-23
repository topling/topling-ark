#define _CRT_SECURE_NO_WARNINGS
#define _SCL_SECURE_NO_WARNINGS
#include <terark/fsa/fsa.hpp>
#include <terark/fsa/dfa_mmap_header.hpp>
#include <terark/fsa/nfa.hpp>
#include <terark/fsa/virtual_machine_dfa.hpp>
#include <terark/bitmap.hpp>
#include <terark/util/autofree.hpp>

using namespace terark;

class DenseDFA_Stat {
public:
	int main(int argc, char* argv[]) {
		if (argc < 2) {
			fprintf(stderr, "Usage: %s dfa_file\n", argv[0]);
			return 1;
		}
		const char* dfaFile = argv[1];
		std::unique_ptr<BaseDFA> dfa(BaseDFA::load_from(dfaFile));
		valvec<size_t> stack;
		febitvec       color(dfa->v_total_states(), 0);
		size_t nil = dfa->v_nil_state();
		if (auto vm = dynamic_cast<const VirtualMachineDFA*>(&*dfa)) {
			for(size_t i = vm->m_dfa_cluster_num; i < vm->num_roots(); ++i) {
				size_t root = vm->get_root(i);
				color.set1(root);
				stack.push_back(root);
			}
			vm->print_vm_stat(stdout);
		}
		else if (dfa->v_state_move(initial_state, 258) == nil) {
			color.set1(initial_state);
			stack.push_back(initial_state);
		}
		else {
			size_t root = initial_state;
			while (nil != (root = dfa->v_state_move(root, 258))) {
				color.set1(root);
				stack.push_back(root);
			}
		}
		size_t sigma = dfa->get_sigma();
		AutoFree<CharTarget<size_t> > moves(sigma);
		AutoFree<size_t> dests(sigma);
		AutoFree<size_t> char_hist(sigma+1, 0);
		AutoFree<size_t> uniq_hist(sigma+1, 0);
		AutoFree<size_t> orig_hist(sigma+1, 0);
		AutoFree<size_t> near_hist(128, 0);
		size_t far__hist = 0; // near < far < long
		size_t long_hist = 0;
		size_t total = 0;
		while (!stack.empty()) {
			size_t parent = stack.back(); stack.pop_back();
			size_t cnt = dfa->get_all_move(parent, moves);
			for (size_t i = 0; i < cnt; ++i) dests[i] = moves[i].target;
			std::sort(dests.p, dests.p + cnt);
			size_t uniq_cnt = std::unique(dests.p, dests.p + cnt) - dests.p;
			for(size_t i = 0; i < uniq_cnt; ++i) {
				size_t child = dests[i];
				if (color.is0(child)) {
					color.set1(child);
					stack.push_back(child);
				}
			}
			if (cnt == 1) {
				intptr_t diff = intptr_t(dests[0]) - intptr_t(parent);
				if (diff >= -64 && diff <= 63) {
					near_hist[64 + diff]++;
				}
				else if (diff >= -(1<<17) && diff < (1<<17)) {
					far__hist++;
				}
				else
					long_hist++;
			}
			if (cnt)
				char_hist[moves[cnt-1].ch - moves[0].ch + 1]++;
			uniq_hist[uniq_cnt]++;
			orig_hist[cnt]++;
			total++;
		}
		size_t sum = 0;
		for(size_t i = 0; i <= sigma; ++i) {
			size_t x = uniq_hist[i];
			sum += x;
			if (x)
				printf("uniq_hist[%03zd]: %8zd  %6.4f  %6.4f\n", i, x, 1.0*x/total, 1.0*sum/total);
		}
		sum = 0;
		for(size_t i = 0; i <= sigma; ++i) {
			size_t x = orig_hist[i];
			sum += x;
			if (x)
				printf("orig_hist[%03zd]: %8zd  %6.4f  %6.4f\n", i, x, 1.0*x/total, 1.0*sum/total);
		}
		sum = 0;
		for(size_t i = 0; i <= sigma; ++i) {
			size_t x = char_hist[i];
			sum += x;
			if (x)
				printf("char_hist[%03zd]: %8zd  %6.4f  %6.4f\n", i, x, 1.0*x/total, 1.0*sum/total);
		}
		sum = 0;
		for(size_t i = 0; i < 128; ++i) {
			size_t x = near_hist[i];
			sum += x;
			if (x)
				printf("near_hist[%+03zd]: %8zd  %6.4f  %6.4f\n", i-64, x, 1.0*x/total, 1.0*sum/total);
		}
		size_t x;
		x = far__hist;
	   	sum += x;
		printf("far__hist[***]: %8zd  %6.4f  %6.4f\n", x, 1.0*x/total, 1.0*sum/total);
		x = long_hist;
	   	sum += x;
		printf("long_hist[***]: %8zd  %6.4f  %6.4f\n", x, 1.0*x/total, 1.0*sum/total);
		return 0;
	}
};

int main(int argc, char* argv[]) {
	return DenseDFA_Stat().main(argc, argv);
}


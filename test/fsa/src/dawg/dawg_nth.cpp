#include <terark/fsa/dawg.hpp>
#include <terark/fsa/onfly_minimize.hpp>
//#include <terark/hash_strmap.hpp>
#include <iostream>
#include <terark/io/DataIO.hpp>
#include <terark/io/FileStream.hpp>
#include <terark/io/StreamBuffer.hpp>
#include <terark/io/DataIO_Exception.hpp>
#include <terark/io/IOException.hpp>
#include <terark/io/IStream.hpp>
#include <terark/util/profiling.hpp>
#include <terark/util/linebuf.hpp>
#include <terark/num_to_str.hpp>
#include "dawg_patch.hpp"

using namespace terark;

int loop_cnt = 10;

template<class Au>
void BenchAccept(const Au& au, const valvec<char>& spool, const valvec<long>& offset) {
	for (long i = 0, n = offset.size()-1; i < n; ++i) {
		const char* s = spool.data() + offset[i];
		long  l = offset[i+1] - offset[i];
		volatile bool b1 = false;
		for (int k = 0; k < loop_cnt; ++k)
			b1 = au.accept(fstring(s, l));
		assert(b1); TERARK_UNUSED_VAR(b1);
	}
}

template<class Au>
void Bench_nth_word(const Au& au, const valvec<char>& spool, const valvec<long>& offset, const valvec<long>& indices) {
	std::string word;
	for (long i = 0, n = indices.size(); i < n; ++i) {
		long  j = indices[i];
		for (int k = 0; k < loop_cnt; ++k)
			au.nth_word(j, &word);
#if !defined(NDEBUG)
		const char* s = spool.data() + offset[i];
		size_t l = offset[i+1] - offset[i];
		assert(word.size() == l && memcmp(s, word.data(), l) == 0);
#endif
	}
}

template<class StateNonZip, class StateZip>
int test() {
	MyDAWG<StateNonZip> dawg1;
	MinADFA_OnflyBuilder<MyDAWG<StateNonZip> > onfly(&dawg1);
	MyDAWG<StateZip> pzip1;
#ifdef TERARK_INST_ALL_DFA_TYPES
	DAWG0<DAWG0_State<StateNonZip> > dawg0;
	MinADFA_OnflyBuilder<DAWG0<DAWG0_State<StateNonZip> > > onfly0(&dawg0);
	DAWG0<DAWG0_State<StateZip> > pzip0;
#endif
    terark::profiling pf;
	valvec<char> spool;
	valvec<long> offset;
	long long t1 = pf.now();
	unsigned lineno = 0;
	terark::LineBuf lb;
	for (; lb.getline(stdin) >= 0; ++lineno) {
		lb.trim();
		if (lb.n) {
			offset.push_back(spool.size());
			spool.append(lb.p, lb.n);
		}
		if (lineno % 100000 == 0)
			fprintf(stderr, "lineno=%09u\n", lineno);
	}
	offset.push_back(spool.size());
	long long t2 = pf.now();
	for (long i = 0, n = offset.size()-1; i < n; ++i) {
		char* s = &spool[offset[i]];
		long  l = offset[i+1] - offset[i];
		onfly.add_word(fstring(s, l));
	}
	printf("Insert completed...\n");
	long long t3 = pf.now();
	pzip1.path_zip(dawg1, "DFS");
	long long t4 = pf.now();
	BenchAccept(dawg1, spool, offset);
	long long t5 = pf.now();
	BenchAccept(pzip1, spool, offset);
	long long t6 = pf.now();
#ifdef TERARK_INST_ALL_DFA_TYPES
	for (long i = 0, n = offset.size()-1; i < n; ++i) {
		char* s = &spool[offset[i]];
		long  l = offset[i+1] - offset[i];
		onfly0.add_word(fstring(s, l));
	}
#endif
	printf("Compiling...\n");
	dawg1.compile();
	pzip1.compile();
#ifdef TERARK_INST_ALL_DFA_TYPES
	dawg0.compile();
	pzip0.path_zip(dawg0, "DFS");
	pzip0.compile();
#endif
	long long t7 = pf.now();
	valvec<long> indices(offset.size()-1);
	for (long i = 0, n = offset.size()-1; i < n; ++i) {
		char* s = &spool[offset[i]];
		long  l = offset[i+1] - offset[i];
		for (int k = 0; k < loop_cnt; ++k)
			indices[i] = pzip1.index(fstring(s, l));
	}
	long long t8 = pf.now();
#ifdef TERARK_INST_ALL_DFA_TYPES
	for (long i = 0, n = offset.size()-1; i < n; ++i) {
		char* s = &spool[offset[i]];
		long  l = offset[i+1] - offset[i];
		long  idx0 = -1;
		for (int k = 0; k < loop_cnt; ++k)
			idx0 = pzip0.index(fstring(s, l));
		assert(idx0 == indices[i]); TERARK_UNUSED_VAR(idx0);
	}
#endif
	long long t9 = pf.now();
	Bench_nth_word(dawg1, spool, offset, indices);
	long long t10 = pf.now();
#ifdef TERARK_INST_ALL_DFA_TYPES
	Bench_nth_word(dawg0, spool, offset, indices);
#endif
	long long t11 = pf.now();
	Bench_nth_word(pzip1, spool, offset, indices);
	long long t12 = pf.now();
#ifdef TERARK_INST_ALL_DFA_TYPES
	Bench_nth_word(pzip0, spool, offset, indices);
#endif
	long long t13 = pf.now();
	long cnt1 = (long)dawg1.dag_pathes();
	long cnt2 = (long)pzip1.dag_pathes();
#define NS_LATENCY(x,y) pf.nf(x,y)/loop_cnt/(offset.size()-1)
#define MB_per_SEC(x,y) loop_cnt*spool.size()/pf.uf(x,y)
    printf("dawg1[dag_pathes=%ld states=%zd transitions=%zd trans/state=%f]\n", cnt1, dawg1.total_states(), dawg1.total_transitions(), (double)dawg1.total_transitions()/dawg1.total_states());
    printf("pzip1[dag_pathes=%ld states=%zd transitions=%zd trans/state=%f]\n", cnt2, pzip1.total_states(), pzip1.total_transitions(), (double)pzip1.total_transitions()/pzip1.total_states());
	printf("mem_size[dawg1=%ld pzip1=%ld]\n", (long)dawg1.mem_size(), (long)pzip1.mem_size());
#ifdef TERARK_INST_ALL_DFA_TYPES
	printf("mem_size[dawg0=%ld pzip0=%ld]\n", (long)dawg0.mem_size(), (long)pzip0.mem_size());
#endif
	printf("time[readfile=%f insert=%f's path_zip=%f's per-insert/find=%f'ns]\n", pf.sf(t1,t2), pf.sf(t2,t3), pf.sf(t3,t4), NS_LATENCY(t2,t2));
	printf("dawg1_accept[total=%f's avg=%f'ns through_put=%fMB/s]\n", pf.sf(t4,t5), NS_LATENCY(t4,t5), MB_per_SEC(t4,t5));
	printf("pzip1_accept[total=%f's avg=%f'ns through_put=%fMB/s]\n", pf.sf(t5,t6), NS_LATENCY(t5,t6), MB_per_SEC(t5,t6));
	printf("\tdawg1/pzip1=%f\n", pf.sf(t4,t5)/pf.sf(t5,t6));
	printf("pzip1_index[total=%f's avg=%f'ns through_put=%fMB/s]\n", pf.sf(t7,t8), NS_LATENCY(t7,t8), MB_per_SEC(t7,t8));
	printf("pzip0_index[total=%f's avg=%f'ns through_put=%fMB/s]\n", pf.sf(t8,t9), NS_LATENCY(t8,t9), MB_per_SEC(t8,t9));
	printf("dawg1_nth_word[total=%f's avg=%f'ns through_put=%fMB/s]\n", pf.sf(t9,t10), NS_LATENCY(t9,t10), MB_per_SEC(t9,t10));
	printf("dawg0_nth_word[total=%f's avg=%f'ns through_put=%fMB/s]\n", pf.sf(t10,t11), NS_LATENCY(t10,t11), MB_per_SEC(t10,t11));
	printf("pzip1_nth_word[total=%f's avg=%f'ns through_put=%fMB/s]\n", pf.sf(t11,t12), NS_LATENCY(t11,t12), MB_per_SEC(t11,t12));
	printf("pzip0_nth_word[total=%f's avg=%f'ns through_put=%fMB/s]\n", pf.sf(t12,t13), NS_LATENCY(t12,t13), MB_per_SEC(t12,t13));
//	dawg1.print_output("dzip_out1.txt");
//	pzip1.print_output("dzip_out2.txt");
//	dawg1.write_dot_file("dawg1.dot");
//	pzip1.write_dot_file("pzip1.dot");
	using namespace terark;
	std::string binfile = "dawg_nth.bin";
	if (1) {
		t1 = pf.now();
		FileStream file(binfile.c_str(), "wb");
		NativeDataOutput<OutputBuffer> out; out.attach(&file);
		out & pzip1;
		t2 = pf.now();
	}
	if (1) {
		MyDAWG<StateZip> pzip2;
		FileStream file(binfile.c_str(), "rb");
		NativeDataInput<InputBuffer> in; in.attach(&file);
		t2 = pf.now();
		in & pzip2;
		assert(pzip2.total_states() == pzip1.total_states());
		assert(memcmp(pzip2.internal_get_pool()  .data(), pzip1.internal_get_pool()  .data(), pzip1.internal_get_pool().size()) == 0);
		assert(memcmp(pzip2.internal_get_states().data(), pzip1.internal_get_states().data(), pzip1.total_states() * sizeof(State5B)) == 0);
		t3 = pf.now();
	}
	printf("time[save=%f load=%f]\n", pf.sf(t1,t2), pf.sf(t2,t3));
	return 0;
}

void usage(const char* prog) {
    fprintf(stderr, "usage: %s [ -b zip_state_bytes -B nonzip_state_bytes -l loop_cnt ]\n", prog);
    exit(3);
}

int main(int argc, char** argv) {
    using namespace std;
	int nonzip_state_bytes = 5;
	int    zip_state_bytes = 5;
    for (;;) {
        int opt = getopt(argc, argv, "B:b:l:");
        switch (opt) {
        default:
            usage(argv[0]);
            break;
        case -1:
            goto GetoptDone;
		case 'B':
			nonzip_state_bytes = atoi(optarg);
			break;
		case 'b':
			zip_state_bytes = atoi(optarg);
			break;
        case 'l':
            loop_cnt = atoi(optarg);
            break;
        }
    }
GetoptDone:
	int ret = 0;
	switch (nonzip_state_bytes * 16 + zip_state_bytes) {
	default:
		fprintf(stderr, "invalid -B nonzip_state_bytes=%d -b zip_state_bytes=%d\n", nonzip_state_bytes, zip_state_bytes);
		break;
	//------------------------------------------------
	case 0x44: ret = test<State4B, State4B>(); break;
	case 0x45: ret = test<State4B, State5B>(); break;
	case 0x46: ret = test<State4B, State6B>(); break;
	case 0x48: ret = test<State4B, State32>(); break;
	//------------------------------------------------
	case 0x54: ret = test<State5B, State4B>(); break;
	case 0x55: ret = test<State5B, State5B>(); break;
	case 0x56: ret = test<State5B, State6B>(); break;
	case 0x58: ret = test<State5B, State32>(); break;
	//------------------------------------------------
	case 0x64: ret = test<State6B, State4B>(); break;
	case 0x65: ret = test<State6B, State5B>(); break;
	case 0x66: ret = test<State6B, State6B>(); break;
	case 0x68: ret = test<State6B, State32>(); break;
	//------------------------------------------------
	case 0x84: ret = test<State32, State4B>(); break;
	case 0x85: ret = test<State32, State5B>(); break;
	case 0x86: ret = test<State32, State6B>(); break;
	case 0x88: ret = test<State32, State32>(); break;
#if TERARK_WORD_BITS >= 64 && defined(TERARK_INST_ALL_DFA_TYPES)
	//------------------------------------------------
	case 0x74: ret = test<State7B, State4B>(); break;
	case 0x75: ret = test<State7B, State5B>(); break;
	case 0x76: ret = test<State7B, State6B>(); break;
	case 0x77: ret = test<State7B, State7B>(); break;
	case 0x78: ret = test<State7B, State32>(); break;
	//------------------------------------------------
	case 0x47: ret = test<State4B, State7B>(); break;
	case 0x57: ret = test<State5B, State7B>(); break;
	case 0x67: ret = test<State6B, State7B>(); break;
	case 0x87: ret = test<State32, State7B>(); break;
#endif
	}
    return ret;
}



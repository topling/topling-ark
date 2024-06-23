#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#define _SCL_SECURE_NO_WARNINGS
#include <io.h>
#include <fcntl.h>
#endif

#include <terark/fsa/dawg.hpp>
#include <terark/fsa/dfa_mmap_header.hpp>
#include <terark/util/profiling.hpp>
#include <terark/io/DataIO.hpp>
#include <terark/io/FileStream.hpp>
#include <terark/io/StreamBuffer.hpp>
#include <terark/hash_strmap.hpp>
#include <getopt.h>

using namespace terark;

bool verbose = false;
const char* child_stats_file_suffix = NULL;

static void StatFile(FILE* ifp, const char* fname)
try {
	std::unique_ptr<BaseDFA> dfa(BaseDFA::load_from(ifp));
	DFA_Stat st;
	typedef long long ll;
	dfa->get_stat(&st);
	printf("HeaderInfo of file(%s):\n", fname);
	printf("dfa_class        : %s\n", st.dfa_class_name);
	printf("version          : %11d\n", st.version);
	printf("num_blocks       : %11d\n", st.num_blocks);
	printf("is_dag           : %11d\n", st.is_dag);
	printf("is_dawg_strpool  : %11d\n", st.is_nlt_dawg_strpool);
	printf("kv_delim         : %#11X\n", st.kv_delim);
	printf("state_size       : %11lld\n", (ll)st.state_size);
	printf("total_states     : %11lld\n", (ll)st.total_states);
	printf("zpath_states     : %11lld\n", (ll)st.zpath_states);
	printf("zpath_length     : %11lld\n", (ll)st.zpath_length);
	printf("numFreeStates    : %11lld\n", (ll)st.numFreeStates);
	printf("firstFreeState   : %11lld\n", (ll)st.firstFreeState);
	printf("transition_num   : %11lld\n", (ll)st.transition_num);
	printf("gnode_states     : %11lld\n", (ll)st.gnode_states);
	printf("atom_dfa_num     : %11lld\n", (ll)st.atom_dfa_num);
	printf("dfa_cluster_num  : %11lld\n", (ll)st.dfa_cluster_num);
	printf("dawg_num_words   : %11lld\n", (ll)st.dawg_num_words);
	printf("louds_dfa: {\n");
	printf("  cache_ptrbit   : %11d\n", st.louds_dfa_cache_ptrbit);
	printf("  cache_states   : %11d\n", st.louds_dfa_cache_states);
	printf("  num_zpath_trie : %11d\n", st.louds_dfa_num_zpath_trie);
	printf("  min_zpath_id   : %11d\n", st.louds_dfa_min_zpath_id);
	printf("  min_cross_dst  : %11d\n", st.louds_dfa_min_cross_dst);
	printf("}\n");
	printf("ac_word_ext      : %11d\n", st.ac_word_ext);
	printf("\n");

	for (unsigned i = 0; i < st.num_blocks; ++i) {
		printf("block[%u]: offset = %11lld    length = %11lld\n",
			i, (ll)st.blocks[i].offset, (ll)st.blocks[i].length);
	}
	std::string str_stat = dfa->str_stat();
	if (!str_stat.empty())
		printf("\nstr_stat:\n%s", str_stat.c_str());

    if (child_stats_file_suffix) {
        auto childstatfile = std::string(fname) + child_stats_file_suffix;
        dfa->write_child_stats(initial_state, "bfs", childstatfile);
    }

	if (!verbose)
		return;

	printf("Verbose detail info...\n");

//	valvec<CharTarget<size_t> > children;
	MatchContext   ctx;
	valvec<size_t> stack;
	valvec<size_t> dests;
	valvec<size_t> pz_hist(512);
	gold_hash_map<long, size_t> tl_hist;
	hash_strmap<size_t> pz_str;
	febitvec color(dfa->v_total_states());
	stack.push_back(initial_state);
	color.set1(initial_state);
	size_t tl_cnt = 0;
	size_t back_ref = 0;
	size_t all_edge_num = 0;
	size_t all_node_num = 0;
	size_t leaf_edge_num = 0;
	size_t tree_edge_num = 0;
	while (!stack.empty()) {
		size_t parent = stack.back(); stack.pop_back();
		dfa->get_all_dest(parent, &dests);
		for (size_t child : dests) {
			if (color.is0(child)) {
				color.set1(child);
				stack.push_back(child);
				tree_edge_num++;
			}
			if (abs(long(child - parent)) >= 256) {
				tl_hist[long(child - parent + 255) & ~255]++;
			} else {
				tl_hist[long(child - parent)]++;
			}
			if (!dfa->v_has_children(child)) leaf_edge_num++;
			if (child < parent) back_ref++;
			all_edge_num++;
		}
		if (dfa->v_is_pzip(parent)) {
			fstring zs = dfa->v_get_zpath_data(parent, &ctx);
			pz_hist[zs.size()]++;
			pz_str[zs]++;
		}
		tl_cnt += dests.size();
		all_node_num++;
	}
	size_t per_state_size = st.state_size;
	size_t pz_size = 0;
	size_t pz_real = 0;
	size_t waste = 0;
	for (size_t i = 0; i < pz_hist.size(); ++i) {
		if (pz_hist[i]) {
			size_t w0 = 0;
			if ((i+1) % per_state_size != 0) {
				w0 = pz_hist[i] * (per_state_size - (i+1)%per_state_size);
				waste += w0;
			}
			pz_real += i * pz_hist[i];
			pz_size += (i+1+per_state_size-1)/per_state_size * per_state_size * pz_hist[i];
		}
	}
	fprintf(stdout, "pz_real=%zd pz_size=%zd pz_num=%zd waste=%zd waste+len=%zd\n",
			pz_real, pz_size, dfa->num_zpath_states(),
		   	waste, waste + dfa->num_zpath_states());
	fprintf(stdout, "pz_avg_real_len=%f\n", 1.0*pz_real/dfa->num_zpath_states());
	fprintf(stdout, "pz_ratio=%f\n", 1.0*dfa->num_zpath_states()/dfa->v_total_states());
	fprintf(stdout, "pz_uniq: bytes=%zd cnt=%zd\n", pz_str.total_key_size(), pz_str.size());
	fprintf(stdout, "non_pz_nodes=%zd ratio=%f\n"
				  , (dfa->v_total_states()-dfa->num_zpath_states())
				  , (dfa->v_total_states()-dfa->num_zpath_states())*1.0/dfa->v_total_states()
		   );
	fprintf(stdout, "back_ref = %zd %f\n", back_ref, back_ref*1.0/tl_cnt);
	fprintf(stdout, "totalstates = %zd\n", dfa->v_total_states());
	fprintf(stdout, "transitions = %zd\n", tl_cnt);
	fprintf(stdout, "transitions-per-state = %f\n", tl_cnt*1.0/dfa->v_total_states());
	fprintf(stdout, "all_node_num = %zd\n", all_node_num);
	fprintf(stdout, "all_edge_num = %zd\n", all_edge_num);
	fprintf(stdout, "edge-per-node = %f\n", all_edge_num*1.0/all_node_num);
	fprintf(stdout, "leaf_edge_num = %zd %f\n", leaf_edge_num, leaf_edge_num*1.0/all_edge_num);
	fprintf(stdout, "tree_edge_num = %zd %f\n", tree_edge_num, tree_edge_num*1.0/all_edge_num);
	fprintf(stdout, "non_tree_edge = %zd %f\n", all_edge_num - tree_edge_num, 1 - tree_edge_num*1.0/all_edge_num);

	fprintf(stdout, "detail:\n");
	for (size_t i = 0; i < pz_hist.size(); ++i) {
		if (pz_hist[i]) {
			size_t w0 = 0;
			if ((i+1) % per_state_size != 0) {
				w0 = pz_hist[i] * (per_state_size - (i+1)%per_state_size);
			}
			size_t pz_inc = (i+1+per_state_size-1)/per_state_size * per_state_size * pz_hist[i];
			pz_size += pz_inc;
			fprintf(stdout, "pzlen=%zd cnt=%zd len*cnt[inc=%zd sum=%zd] waste=%zd\n"
					, i, pz_hist[i], pz_inc, pz_size, w0);
		}
	}

	tl_hist.sort([](std::pair<long,size_t> x
				  , std::pair<long,size_t> y){return x.second > y.second;});
	size_t accu = 0;
	for (size_t i = 0; i < tl_hist.size(); ++i) {
		accu += tl_hist.val(i);
		fprintf(stdout, "tl_diff = %ld cnt = %zd %f accu = %zd %f\n",
			   	tl_hist.key(i), tl_hist.val(i), tl_hist.val(i)*1.0/tl_cnt, accu, accu*1.0/tl_cnt);
	}

	const SuffixCountableDAWG* dawg = dynamic_cast<const SuffixCountableDAWG*>(&*dfa);
	if (dawg) {
		color.fill(0);
		color.set1(initial_state);
		stack.push_back(initial_state);
		while (!stack.empty()) {
			size_t parent = stack.back(); stack.pop_back();
			dfa->get_all_dest(parent, &dests);
			for (size_t child : dests) {
				if (color.is0(child)) {
					color.set1(child);
					stack.push_back(child);
				}
			}
			fprintf(stdout, "suffix_cnt: %zd\n", dawg->suffix_cnt(parent));
		}
	}
}
catch (const std::exception& ex) {
	fprintf(stderr, "caught: %s\n", ex.what());
}

void usage(const char* prog) {
	fprintf(stderr, R"EOS(Usage: %s Options dfa-files
Options:
    -h Show this help information
    -c filename
         write_child_stats(bfs), text file

If dfa file is omitted, use stdin
)EOS", prog);
	exit(1);
}

int main(int argc, char* argv[]) {
	valvec<const char*> ifiles;
	for (;;) {
		int opt = getopt(argc, argv, "c:v");
		switch (opt) {
		case -1:
			goto GetoptDone;
		case '?':
			return 1;
        case 'c':
            child_stats_file_suffix = optarg;
            break;
		case 'v':
			verbose = true;
			break;
		}
	}
GetoptDone:
	for (int j = optind; j < argc; ++j) {
		ifiles.push_back(argv[j]);
	}
	valvec<Auto_fclose> ifp(ifiles.size(), valvec_reserve());
	if (!ifiles.empty()) {
		for (size_t j = 0; j < ifiles.size(); ++j) {
			FILE* fp = fopen(ifiles[j], "rb");
			if (NULL == fp) {
				fprintf(stderr, "ERROR: fopen(%s, rb) = %s\n", ifiles[j], strerror(errno));
				return 1;
			}
			ifp.emplace_back(fp);
		}
	}
	else {
	#ifdef _MSC_VER
		_setmode(_fileno(stdin), _O_BINARY);
	#endif
		ifp.emplace_back(nullptr); // stdin
	}
	for (size_t j = 0; j < ifp.size(); ++j) {
		if (NULL == ifp[j])
			StatFile(stdin, "/dev/stdin");
		else
			StatFile(ifp[j], ifiles[j]);
		printf("\n");
	}
	return 0;
}


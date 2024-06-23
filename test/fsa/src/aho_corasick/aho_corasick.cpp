#define _SCL_SECURE_NO_WARNINGS // fuck vc
#define _CRT_SECURE_NO_WARNINGS
#define _CRT_NONSTDC_NO_WARNINGS
#if defined(_WIN32) || defined(_WIN64)
	#define NOMINMAX
	#include <Windows.h> // for Memory Mapping API
	const int STDIN_FILENO = 0;
	const int STDOUT_FILENO = 1;
#else
//	#define _GNU_SOURCE
	#include <unistd.h>
	#include <sys/mman.h>
#endif
#include <terark/fsa/aho_corasick.hpp>
#include <terark/fsa/double_array_trie.hpp>
#include <terark/fsa/dawg.hpp>
#include <terark/util/profiling.hpp>
#include <terark/util/linebuf.hpp>

#include <iostream>
#include <fstream>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

using namespace terark;

struct Compare_end {
    template<class T>
    bool operator()(const T& x, const T& y) const { return x.second < y.second; }
};

template<class T>
int diff(valvec<T>& v, size_t i) { assert(i+1<v.size()); return int(v[i+1]-v[i]); }

struct Main {
	typedef Aho_Corasick<DoubleArrayTrie<AC_State<DA_State8B> > > da_ac_t;
    typedef Aho_Corasick<Automata       <AC_State<State32   > > > au_ac_t;
	typedef AC_FullDFA<uint32_t> MyFullDFA;
	au_ac_t au_ac;
	da_ac_t da_ac;
	MyFullDFA fulldfa;
	valvec<unsigned> offsets;
	valvec<char>     strpool;
	terark::profiling pf;
	bool sort_by_beg, quiet;
	bool with_full_dfa = false;
	size_t total_len;
	int use_mmap = 0;
	int loop_cnt = 1;
	const char* BFS_or_DFS = "DFS";

	int build(const char* fname_pattern) {
		size_t lineno = 0;
		long long t0, t1, t2, t3, t4;
	    {
			// read the patterns (key words)
			terark::Auto_fclose fp(fopen(fname_pattern, "r"));
			if (NULL == fp) {
				fprintf(stderr, "failed: fopen(%s, r) = %s\n", fname_pattern, strerror(errno));
				return 3;
			}
			puts("Read key words:");
			terark::LineBuf line;
			au_ac_t::ac_builder builder(&au_ac);
			t0 = pf.now();
			while (line.getline(fp) >= 0) {
				line.trim();
				if (line.n) {
				//  printf("\t%s\n", line.c_str());
					builder.add_word(line);
				}
				lineno++;
			}
			t1 = pf.now();
		//	da_ac.print_output();
			builder.set_word_id_as_lex_ordinal(&offsets, &strpool);
			t2 = pf.now();
			fprintf(stderr, "compiling Aho-Corasick Automata\n");
			builder.compile();
			builder.sort_by_word_len(offsets);
		}
		t3 = pf.now();
		double_array_ac_build_from_au(da_ac, au_ac, BFS_or_DFS);
		t4 = pf.now();
		fprintf(stderr, "compile success, words=%ld states=%ld, transition=%ld, mem_size=%ld\n",
				(long)offsets.size()-1,
				(long)au_ac.total_states(),
				(long)au_ac.total_transitions(),
				(long)au_ac.mem_size());
		fprintf(stderr, "double_array_trie: states=%ld, transition=%ld, mem_size=%ld\n",
				(long)da_ac.total_states(),
				(long)da_ac.total_transitions(),
				(long)da_ac.mem_size());
		fprintf(stderr, "time: read+add=%f set_lex_ordinal+copy=%f compile=%f\n"
				, pf.sf(t0,t1), pf.sf(t1,t2), pf.sf(t2,t3)
				);
		fprintf(stderr, "time: double_array_build(%s)=%f load_factor=%f\n"
				, BFS_or_DFS
				, pf.sf(t3,t4)
				, double(au_ac.total_states()-1)/(da_ac.total_states()-1));
		fprintf(stderr, "time: total-build = %f\n", pf.sf(t0,t4));
		fprintf(stderr, "---------------------\n");
		if (with_full_dfa) {
			fprintf(stderr, "Building AC_FullDFA<uint32_t>...\n");
			FullDFA_BaseOnAC<da_ac_t> vfull(&da_ac);
			uint32_t src_root = initial_state;
			fulldfa.build_from(vfull, &src_root, 1);
			long long t5 = pf.now();
			fprintf(stderr, "time: AC_FullDFA<uint32_t>=%f\n", pf.sf(t4,t5));
		}
		return 0;
	}

	template<class ACT>
	int line_by_line(const ACT& ac, const char* fname) {
		printf("%s\n", BOOST_CURRENT_FUNCTION);
		std::istream* target;
		if (NULL == fname) {
			fprintf(stderr, "Reading target texts from stdin\n");
			target = &std::cin;
		} else {
			std::ifstream* targetf = new std::ifstream(fname);
			if (!targetf->is_open()) {
				fprintf(stderr, "can not open: %s\n", fname);
				return 3;
			}
			target = targetf;
		}
		printf("Scan text line by line:\n");
		valvec<std::pair<unsigned,unsigned> > match;
		size_t lineno = 0;
		total_len = 0;
		std::string line;
		size_t hits = 0;
		while (std::getline(*target, line)) {
			++lineno;
			total_len += line.size();
			for (int i = 0; i < loop_cnt; ++i) {
				match.resize(0); // don't change capacity
				ac.tpl_ac_scan(line, &match);
				hits += match.size();
			}
			if (quiet) { // don't print match result, just report progress
				if (lineno % 10000 == 0) {
					intptr_t w = ::write(STDOUT_FILENO, ".", 1);
					assert(w); TERARK_UNUSED_VAR(w);
				}
				continue;
			}
			printf("len=%02d, text=%s", (int)line.size(), line.c_str());
			if (match.empty()) {
				printf("\t:NO match\n");
			}
			else {
				printf("\n");
				if (sort_by_beg) {
					for (int i = 0, n = match.size(); i < n; ++i) {
						auto& one = match[i];
						one.second -= diff(offsets, one.first);
					}
					std::sort(match.begin(), match.end(), Compare_end());
					for (int i = 0, n = match.size(); i < n; ++i) {
						auto& one = match[i];
						const char* key = &strpool[0]+offsets[one.first];
						const int   len = diff(offsets, one.first);
						printf("%6d %.*s\n", one.second, len, key);
					}
				} else {
					for (int i = 0, n = match.size(); i < n; ++i) {
						auto& one = match[i];
						const char* key = &strpool[0]+offsets[one.first];
						const int   len = diff(offsets, one.first);
						printf("%6d %.*s\n", one.second, len, key);
					}
				}
			}
		}
		if (target != &std::cin)
			delete target;
		fprintf(stderr, "\nhits=%ld\n", long(hits));
		return 0;
	}

	template<class ACT>
	int whole_by_mmap(const ACT& ac, const char* fname) {
		printf("%s\n", BOOST_CURRENT_FUNCTION);
		int fd = STDIN_FILENO;
		if (NULL == fname) {
			fname = "/dev/stdin";
		} else {
			fd = ::open(fname, O_RDONLY);
			if (fd < 0) {
				fprintf(stderr, "::open(\"%s\") = %s\n", fname, strerror(errno));
				return 1;
			}
		}
#if (defined(_WIN32) || defined(_WIN64)) && !defined(__CYGWIN__)
		long long fileSize = _filelengthi64(fd);
		fprintf(stderr, "file_size=%lld\n", fileSize);
		HANDLE hFile = (HANDLE)::_get_osfhandle(fd);
		HANDLE hMmap = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
		if (NULL == hMmap) {
			DWORD err = GetLastError();
			THROW_STD(runtime_error, "CreateFileMapping().ErrCode=%d(0x%X)", err, err);
		}
		char* base = (char*)MapViewOfFile(hMmap, FILE_MAP_READ, 0, 0, 0);
		if (NULL == base) {
			DWORD err = GetLastError();
			CloseHandle(hMmap);
			THROW_STD(runtime_error, "MapViewOfFile().ErrCode=%d(0x%X)", err, err);
		}
		CloseHandle(hMmap);
#else
		struct stat st;
		if (::fstat(fd, &st) < 0) {
			fprintf(stderr, "fstat(\"%s\") = %s\n", fname, strerror(errno));
			return 1;
		}
		long long fileSize = st.st_size;
		fprintf(stderr, "file_size(%s) = %lld\n", fname, fileSize);
		int flags = MAP_SHARED;
		int offset = 0;
		char* base = (char*)mmap(NULL, st.st_size, PROT_READ, flags, fd, offset);
		if (MAP_FAILED == base) {
			fprintf(stderr, "mmap(\"%s\", PROT_READ) = %s\n", fname, strerror(errno));
			if (STDIN_FILENO != fd) {
				::close(fd);
				return 1;
			}
		}
#endif
		size_t hits = 0;
		if (quiet) {
			if (use_mmap <= 1) {
				const char* end = base + fileSize;
				const char* cur = base;
				while (cur < end) {
					const char* line_end = std::find(cur, end, '\n');
					for (int i = 0; i < loop_cnt; ++i)
						ac.tpl_ac_scan(fstring(cur, line_end), [&](size_t, const unsigned*, size_t n, size_t) { hits += n; });
					cur = line_end + 1;
				}
			} else
				for (int i = 0; i < loop_cnt; ++i)
					ac.tpl_ac_scan(fstring(base, fileSize), [&](size_t, const unsigned*, size_t n, size_t) { hits += n; });
		} else {
		  for (int i = 0; i < loop_cnt; ++i)
			ac.tpl_ac_scan(fstring(base, fileSize), [&](size_t endpos, const unsigned* first_word_id, size_t n, size_t) {
				hits += n;
				int wid = first_word_id[0];
				int maxlen = int(offsets[wid+1] - offsets[wid]);
				printf("endpos=%d maxlen=%d----\n", int(endpos), maxlen);
				for (size_t i = 0; i < n; ++i) {
					wid = first_word_id[i];
					int len = int(offsets[wid+1] - offsets[wid]);
					printf("  %*.*s\n", maxlen, len, base + endpos - len);
				}
			});
		}
		total_len = fileSize;
#if (defined(_WIN32) || defined(_WIN64)) && !defined(__CYGWIN__)
		UnmapViewOfFile(base);
#else
		munmap(base, fileSize);
#endif
		fprintf(stderr, "hits=%ld\n", long(hits));
		return 0;
	}

	void usage(const char* prog) {
		fprintf(stderr
			,
"usage: %s options\n"
"	-q : QUIET, don't print match result\n"
"	-p pattern_file\n"
"	-f text_file for scan\n"
"	-m 0, 1 or 2, default is 0, use mmap\n"
"      0: do not use mmap\n"
"      1: use mmap, scan line by line\n"
"      2: use mmap, scan all data once per-loop\n"
"	-d : use double array trie for AC automata\n"
"	-F : use Full-DFA AC automata\n"
"	-l loop_cnt : used for amortize non-scan overhead\n"
			, prog);
	}

int main(int argc, char** argv) {
    using namespace std;

	quiet = false;
    // default is sort_by_beg
    // use env sort_by_beg=0 to disable sort
    const char* sz = getenv("sort_by_beg");
    sort_by_beg = NULL == sz || atoi(sz) != 0;
	bool use_double_array = false;
	const char* fname = NULL;
	const char* fname_pattern = NULL;
	while (true) {
		int opt = getopt(argc, argv, "l:f:Fhp:qm::d::");
		switch (opt) {
		default:
			usage(argv[0]);
			return 3;
		case -1:
			goto ArgParseDone;
		case '?':
			fprintf(stderr, "invalid option: %c\n", optopt);
			usage(argv[0]);
			return 3;
		case 'h':
			usage(argv[0]);
			return 0;
		case 'd':
			// -d has optional arg for walker type: "BFS" or "DFS"
			if (optarg)
				BFS_or_DFS = optarg;
			use_double_array = true;
			break;
		case 'f':
			fname = optarg;
			break;
		case 'F':
			with_full_dfa = true;
			break;
		case 'p':
			fname_pattern = optarg;
			break;
		case 'q':
			quiet = true;
			break;
		case 'm':
			use_mmap = optarg ? atoi(optarg) : 1;
			break;
		case 'l':
			loop_cnt = std::max(atoi(optarg), 1);
			break;
		}
	}
ArgParseDone:
	if (NULL == fname_pattern) {
		fprintf(stderr, "missing -p pattern_file\n");
		usage(argv[0]);
		return 3;
	}
	int ret = build(fname_pattern);
	if (ret != 0)
		return ret;
	long long t0 = pf.now();
	if (use_mmap) {
		if (use_double_array)
			whole_by_mmap(da_ac, fname);
		else
			whole_by_mmap(au_ac, fname);
	}
	else {
		if (use_double_array)
			line_by_line(da_ac, fname);
		else
			line_by_line(au_ac, fname);
	}
	long long t1 = pf.now();
    fprintf(stderr, "End. time=%f speed=%fMB/S\n", pf.sf(t0,t1), double(total_len)*loop_cnt/pf.us(t0,t1));

	if (with_full_dfa) {
		fprintf(stderr, "Benchmarking FullAC...\n");
		long long t2 = pf.now();
		if (use_mmap) {
			whole_by_mmap(fulldfa, fname);
		}
		else {
			line_by_line(fulldfa, fname);
		}
		long long t3 = pf.now();
		fprintf(stderr, "End. time=%f speed=%fMB/S\n", pf.sf(t2, t3), double(total_len)*loop_cnt/pf.us(t2, t3));
		fprintf(stderr, "Speed Ratio(FullAC/BaseAC) = %f\n", double(t1-t0)/(t3-t2));
	}
	return 0;
}

};

int main(int argc, char** argv) {
	return Main().main(argc, argv);
}

#include <terark/fsa/dawg.hpp>
#include <terark/fsa/onfly_minimize.hpp>
#include <terark/hash_strmap.hpp>
#include <terark/util/fstrvec.hpp>
#include <terark/util/linebuf.hpp>
#include <terark/io/file_load_save.hpp>
#include <terark/io/DataIO.hpp>
#include <terark/io/FileStream.hpp>
#include <terark/io/StreamBuffer.hpp>
#include <getopt.h>

using namespace terark;

struct ip_elem_t {
	uint32_t lo, hi;
	uint32_t loc; // idx of loc
	DATA_IO_LOAD_SAVE(ip_elem_t, &lo &hi &loc);

	friend bool operator<(const ip_elem_t& x, const ip_elem_t& y) { return x.hi < y.hi; }
	friend bool operator<(const uint32_t x, const ip_elem_t& y) { return x < y.hi; }
	friend bool operator<(const ip_elem_t& x, const uint32_t y) { return x.hi < y; }
};

struct ip_db_t {
	DAWG<State5B>     names;
	valvec<ip_elem_t> index;

	const ip_elem_t* begin() const { return index.begin(); }
	const ip_elem_t* end  () const { return index.end  (); }
	size_t size() const { return index.size(); }

	const ip_elem_t& operator[](size_t i) const { assert(i < index.size()); return index[i]; }

	void swap(ip_db_t& y) {
		names.swap(y.names);
		index.swap(y.index);
	}

	void load_text(const char* fname) {
		terark::FileStream file(fname, "r");
		load_text(file);
	}
	void load_text(FILE* fp) {
		unsigned lineno = 0;
		fstrvec  svec;
		terark::LineBuf line;
		valvec<ip_elem_t> ipv;
		for (; line.getline(fp) >= 0; ++lineno) {
			line.trim();
			uint32_t i1, i2, i3, i4, i5, i6, i7, i8;
			int numb = -1;
			int fields = sscanf(line, "%u.%u.%u.%u %u.%u.%u.%u%*[ ]%n", &i1, &i2, &i3, &i4, &i5, &i6, &i7, &i8, &numb);
			if (fields == 8 && -1 != numb) {
				uint32_t iplo = i1 << 24 | i2 << 16 | i3 << 8 | i4;
				uint32_t iphi = i5 << 24 | i6 << 16 | i7 << 8 | i8;
				if (iphi != 0xFFFFFFFF)
					iphi += 1;
				ipv.push_back({iplo, iphi, (uint32_t)svec.size()});
				svec.emplace_back(line+numb, line.end());
			}
			else {
				fprintf(stderr, "fields=%d, numb=%d  invalid line(%u): %s\n", fields, numb, lineno, line.p);
			}
			if (lineno % 100000 == 0) {
				printf("lineno=%09u\n", lineno);
			}
		}
		DAWG<State5B> pzip;
		printf("read completed, insert to dawg\n");
		assert(svec.size() == ipv.size());
		{
			MinADFA_OnflyBuilder<DAWG<State5B> > onfly(&pzip);
			for (long i = 0, n = svec.size(); i < n; ++i) {
				fstring s = svec[i];
				onfly.add_word(s);
				if (i % 10000 == 0) {
					printf("."); fflush(stdout);
				}
			}
			printf("insert completed, path_zip & compile\n");
			pzip.path_zip("DFS");
			pzip.compile();
		}
		printf("set loc_idx\n");
		for (long i = 0, n = svec.size(); i < n; ++i) {
			size_t loc_idx = pzip.index(svec[i]);
			assert(pzip.null_word != loc_idx);
			ipv[i].loc = loc_idx;
		}
		printf("sort by high ip\n");
		std::sort(ipv.begin(), ipv.end());
		names.swap(pzip);
		index.swap(ipv);
	}

	void save_text(const char* fname) const {
		fprintf(stderr, "save_text is not implemented yet\n");
	}

	DATA_IO_LOAD_SAVE_V(ip_db_t, 1, &names &index);
};

int main(int argc, char** argv) {
    using namespace std;
	const char* bin_i_fname = NULL;
	const char* bin_o_fname = NULL;
	const char* txt_i_fname = NULL;
//	const char* txt_o_fname = NULL;
	for (int opt; (opt = getopt(argc, argv, "t:T:b:B:")) != -1; ) {
		switch (opt) {
		case 't': txt_i_fname = optarg; break;
	//	case 'T': txt_o_fname = optarg; break;
		case 'b': bin_i_fname = optarg; break;
		case 'B': bin_o_fname = optarg; break;
		}
	}

	ip_db_t  ipdb;
	if (bin_i_fname)
		terark::native_load_file(bin_i_fname, &ipdb);
	else if (txt_i_fname)
		ipdb.load_text(txt_i_fname);

    printf("pzip[dag_pathes=%ld states=%ld]\n", ipdb.names.dag_pathes(), (long)ipdb.names.total_states());
	printf("mem_size.pzip=%ld\n", (long)ipdb.names.mem_size());

	for (int i = optind; i < argc; ++i) {
		uint32_t i1, i2, i3, i4;
		const char* ip_str = argv[i];
		int fields = sscanf(ip_str, "%u.%u.%u.%u", &i1, &i2, &i3, &i4);
		if (fields == 4) {
			uint32_t ip = i1 << 24 | i2 << 16 | i3 << 8 | i4;
			size_t f = std::upper_bound(ipdb.begin(), ipdb.end(), ip) - ipdb.begin();
			if (ipdb.size() != ip && ip >= ipdb[f].lo) {
				std::string name = ipdb.names.nth_word(ipdb[f].loc);
				printf("ip=%-16s  loc=%s\n", ip_str, name.c_str());
			} else {
				fprintf(stderr, "ip=%-20s  Not Found\n", ip_str);
			}
		} else {
			fprintf(stderr, "invalid ip: %s\n", ip_str);
		}
	}

	if (bin_o_fname)
		terark::native_save_file(bin_o_fname, ipdb);
    return 0;
}



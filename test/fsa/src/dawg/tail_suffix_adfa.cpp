#include <terark/fsa/dawg.hpp>
#include <terark/fsa/onfly_minimize.hpp>
#include <iostream>
#include <terark/io/DataIO.hpp>
#include <terark/io/FileStream.hpp>
#include <terark/io/StreamBuffer.hpp>
#include <terark/io/DataIO_Exception.hpp>
#include <terark/io/IOException.hpp>
#include <terark/io/IStream.hpp>
#include <terark/util/profiling.hpp>
#include <terark/util/linebuf.hpp>
#include <terark/util/autoclose.hpp>
#include <terark/num_to_str.hpp>
#include <terark/hash_strmap.hpp>

using namespace terark;

void usage(const char* prog) {
	fprintf(stderr, "usage: %s -o adfa_out_file -s sorted_by_hash_table_output\n", prog);
	exit(3);
}

int main(int argc, char* argv[]) {
	const char* ofname = NULL;
	const char* sfname = NULL;
    for (;;) {
        int opt = getopt(argc, argv, "o:s:");
        switch (opt) {
        default:
            usage(argv[0]);
            break;
        case -1:
            goto GetoptDone;
        case 's':
            sfname = optarg;
            break;
        case 'o':
            ofname = optarg;
            break;
        }
    }
GetoptDone:
	terark::Auto_fclose fpo, fps;
	if (ofname) {
		fpo = fopen(ofname, "w");
		if (!fpo) {
			fprintf(stderr, "ERROR: fopen(%s, w) = %s\n", ofname, strerror(errno));
			return 1;
		}
	}
	if (sfname) {
		fps = fopen(sfname, "w");
		if (!fps) {
			fprintf(stderr, "ERROR: fopen(%s, w) = %s\n", sfname, strerror(errno));
			return 1;
		}
	}
	terark::LineBuf line;
	valvec<fstring> F;
	hash_strmap<valvec<std::string> > prefix;
	while (line.getline(stdin) > 0) {
		line.chomp();
		line.split(' ', &F);
		auto& vec = prefix[F[0]];
		for (size_t i = 1; i < F.size(); ++i) {
			vec.push_back(F[i].str());
			std::sort(vec.begin(), vec.end());
		}
	}
	prefix.sort_fast();
	typedef Automata<State32> DFA;
	DFA dfa1, dfa2;
	MinADFA_OnflyBuilder<DFA> builder1(&dfa1);
	MinADFA_OnflyBuilder<DFA> builder2(&dfa2);
	for (size_t i = 0; i < prefix.end_i(); ++i) {
		fstring key = prefix.key(i);
		dfa2.clear();
		builder2.reset_root_state(0);
		const auto& vec = prefix.val(i);
		for (size_t j = 0; j < vec.size(); ++j) {
			builder2.add_word(vec[j]);
			fprintf(fps.self_or(stdout), "%s\t%s\n", key.p, vec[j].c_str());
		}
		builder2.close();
		auto root = dfa1.copy_from(dfa2);
		builder1.add_suffix_adfa(key, '\t', root);
	}
	if (fpo == fps) {
		fprintf(fpo.self_or(stdout), "------------------\n");
	}
	dfa1.print_output(fpo.self_or(stdout));
	return 0;
}


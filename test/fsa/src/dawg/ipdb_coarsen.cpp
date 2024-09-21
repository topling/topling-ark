#include <terark/valvec.hpp>
#include <terark/fsa/aho_corasick.hpp>
#include <terark/hash_strmap.hpp>
#include <terark/util/fstrvec.hpp>
#include <terark/util/linebuf.hpp>
#include <terark/io/FileStream.hpp>
#include <getopt.h>

using namespace terark;

int main(int argc, char** argv) {
	const char* city_list_fname = NULL;
	for (int opt; (opt = getopt(argc, argv, "l:")) != -1; ) {
		switch (opt) {
			case 'l': city_list_fname = optarg; break;
		}
	}
	if (NULL == city_list_fname) {
		fprintf(stderr, "-l city_list_fname is required argument\n");
		return 3;
	}
	typedef Aho_Corasick<Automata<AC_State<State5B> > > ac_t;
	ac_t ac;
	terark::LineBuf line;
	valvec<fstring> F;
	fstrvec svec;
	{
		ac_t::ac_builder builder(&ac);
		FileStream city_list_file(city_list_fname, "r");
		while (line.getline(city_list_file) > 0) {
			line.trim();
			builder.add_word(line);
		}
		svec.reserve_strpool(builder.all_words_size);
		builder.set_word_id_as_lex_ordinal(&svec);
		builder.compile();
	}
	while (line.getline(stdin) > 0) {
		fputs(line.p, stdout);
		line.trim();
		line.split(' ', &F, 3);
		if (F.size() < 3) {
			fprintf(stderr, "invalid: fields=%zd\n", F.size());
			continue;
		}
		fstring geo_name = F[2];
		ac.tpl_ac_scan(geo_name,
		  [&](size_t,const unsigned* words, size_t num, size_t) {
		    for (size_t i = 0; i < num; ++i) {
				auto word_id = words[i];
				fstring word = svec[word_id];
				printf("\tMatch: %.*s\n", int(word.n), word.p);
			}
		});
	}
    return 0;
}



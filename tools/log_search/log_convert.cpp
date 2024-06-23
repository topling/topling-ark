#include <stdio.h>
#include <string.h>
#include <terark/hash_strmap.hpp>
#include <terark/lcast.hpp>
#include <terark/fsa/mre_match.hpp>
#include <terark/util/autoclose.hpp>
#include <terark/util/linebuf.hpp>
#include <terark/util/unicode_iterator.hpp>
#include <getopt.h>

using namespace terark;

int usage(const char* prog) {
	fprintf(stderr, R"EOS(Usage:
   %s Options [Input-TXT-File]

Description:
   Build DFA from Input-TXT-File, If Input-TXT-File is omitted, use stdin.
   This program is named adfa_build is because of the DFA graph is Acyclic.

Options:
   -h : Show this help infomation
   -q : Be quiet, don't print progress info
)EOS"
	, prog);
	return (3);
}

struct Main {

bool be_quiet = false;
const char* regex_bin_meta_fname = NULL;
const char* regex_dfa_fname = NULL;
std::unique_ptr<MultiRegexSubmatch> submatch;
hash_strmap<int> regex_name2id;

void log_convert(const char* fname) {
	terark::Auto_fclose fp(fopen(fname, "r"));
	terark::LineBuf line;
	while (line.getline(fp) > 0) {
		line.chomp();
		const char* pos = line.begin();
		const char* end = line.end();
		while (pos < end) {
			int max_match_len = submatch->match_utf8(fstring(pos, end), gtab_ascii_tolower);
			for(int regex_id : submatch->fullmatch_regex()) {
				int nsub = submatch->num_submatch(regex_id);
				switch (regex_id) {
				case 0: // KeyValuePair
					if (nsub >= 3) {
						fstring key = (*submatch)(pos, regex_id, 1);
						fstring val = (*submatch)(pos, regex_id, 2);
						printf("%.*s:%.*s\t%s\tsource:%s\n"
							, key.ilen(), key.data(), val.ilen(), val.data()
							, line.p, fname
							);
					}
					else {
						fprintf(stderr, "FATAL: KeyValuePair, unexpected nsub=%d\n", nsub);
					}
					break;
				case 1: // IpAddress
					printf("ip:%.*s\t%s\tsource:%s\n", max_match_len, pos, line.p, fname);
					break;
				case 2: // date-yyyy-m-d-H-M-S
					{
						/*
						fstring yy = (*submatch)(line.p, regex_id, 0);
						fstring mm = (*submatch)(line.p, regex_id, 1);
						fstring dd = (*submatch)(line.p, regex_id, 2);
						fstring hh = (*submatch)(line.p, regex_id, 3);
						fstring MM = (*submatch)(line.p, regex_id, 4);
						fstring ss = (*submatch)(line.p, regex_id, 5);
						*/
						printf("datetime:%.*s\t%s\tsource:%s\n", max_match_len, pos, line.p, fname);
					}
					break;
				case 3:
					if (nsub >= 2) {
						fstring title = (*submatch)(pos, regex_id, 1);
						while (title.n && title.end()[-1] != ')') title.n--;
						title.n--;
						printf("title:%.*s\t%s\tsource:%s\n", title.ilen(), title.data(), line.p, fname);
					}
					else {
						fprintf(stderr, "FATAL: title, unexpected nsub=%d\n", nsub);
					}
					break;
				}
			}
			if (max_match_len) {
				pos += max_match_len;
			}
			else {
				pos += terark::utf8_byte_count(*pos);
			}
		}
	}
}

void parse_regex_name2id() {
	Auto_fclose bin_meta_fp(fopen(regex_bin_meta_fname, "r"));
	if (NULL == bin_meta_fp) {
		fprintf(stderr, "FATAL: fopen(%s, r) = %s\n", regex_bin_meta_fname, strerror(errno));
		exit(1);
	}
	terark::LineBuf line;
	terark::valvec<fstring> fields;
	while (line.getline(bin_meta_fp) > 0) {
		line.chomp();
		line.split('\t', &fields);
		if (fields.size() < 6) {
			fprintf(stderr, "FATAL: bin_meta_file: %s, regex name is missing\n", regex_bin_meta_fname);
			exit(3);
		}
		int regex_id = lcast(fields[0]);
		regex_name2id[fields[5]] = regex_id;
	}
}

int main(int argc, char* argv[]) {
	for (;;) {
		int opt = getopt(argc, argv, "hqd:r:");
		switch (opt) {
		default:
		case 'h':
			return usage(argv[0]);
		case -1:
			goto GetoptDone;
		case 'q':
			be_quiet = true;
			break;
		case 'd':
			regex_dfa_fname = optarg;
			break;
		case 'r':
			regex_bin_meta_fname = optarg;
			break;
		}
	}
GetoptDone:
	if (NULL == regex_dfa_fname) {
		fprintf(stderr, "-d regex_dfa_file is required");
		return usage(argv[0]);
	}
	if (NULL == regex_bin_meta_fname) {
		fprintf(stderr, "-r regex_bin_meta_file is required");
		return usage(argv[0]);
	}
	submatch.reset(new MultiRegexSubmatch(regex_dfa_fname, regex_bin_meta_fname));
//	parse_regex_name2id();
	for (int i = optind; i < argc; ++i) {
		const char* fname = argv[i];
		log_convert(fname);
	}
	return 0;
}

};

int main(int argc, char* argv[]) {
	return Main().main(argc, argv);
}


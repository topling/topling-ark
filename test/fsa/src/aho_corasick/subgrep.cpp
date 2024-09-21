#include <terark/fsa/aho_corasick.hpp>
#include <terark/fsa/iterator_to_byte_stream.hpp>
#include <terark/util/linebuf.hpp>
#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>

#define is_utf8_head1(c) (((c) & 0xe0) == 0xc0)
#define is_utf8_head2(c) (((c) & 0xf0) == 0xe0)
#define is_utf8_tail(c)  (((c) & 0xc0) == 0x80)

#define char_len(c) (is_utf8_head2(c) ? 3 : is_utf8_head1(c) ? 2 : 1)

using namespace terark;

void usage(const char* prog) {
	fprintf(stderr, "usage: %s {phrase}\n", prog);
	exit(1);
}

void strlower(char* str, size_t len) {
	for (size_t i = 0; i < len;) {
		char c = str[i];
		if (is_utf8_head2(c))
			i += 3;
		else if (is_utf8_head1(c))
			i += 2;
		else if (is_utf8_tail(c))
			i += 1;
		else
			str[i] = tolower(c), ++i;
	}
}
void strlower(std::string& str) {
	 strlower(&str[0], str.size());
}

int strlen_utf8(const char* str) {
	int ulen = 0;
	for (int i = 0; str[ulen]; ++ulen) {
		char c = str[i];
		if (is_utf8_head2(c)) {
			if (0 == str[1] || 0 == str[2])
				return ulen;
			i += 3;
		}
		else if (is_utf8_head1(c)) {
			if (0 == str[1])
				return ulen;
			i += 2;
		} else
			i += 1;
	}
	return ulen;
}

int main(int argc, char* argv[]) {
	if (argc < 2) {
		usage(argv[0]);
		return 1;
	}
	valvec<std::string> fnames;
	bool ignore_case = false;
	for (;;) {
		int ret = getopt(argc, argv, "if:");
		switch (ret) {
		default:
			fprintf(stderr, "getopt.ret=%d\n", ret);
			break;
		case -1:
			goto ParseDone;
		case 'f':
			fnames.push_back(optarg);
			break;
		case 'i':
			ignore_case = true;
			break;
		}
	}
ParseDone:
	if (fnames.empty()) {
		fnames.push_back("/dev/stdin");
	}
	valvec<unsigned> offsets;
	typedef Aho_Corasick<Automata<AC_State<State5B> > > ac_t;
	ac_t ac; {
		ac_t::ac_builder builder(&ac);
		for (int k = optind; k < argc; ++k) {
			const char* qry = argv[k];
			int minlen = 2;
			if (':' == qry[0]) {
				char* next = NULL;
				minlen = strtoul(qry+1, &next, 10);
				qry = next+1;
				if (0 == minlen)
					minlen = strlen_utf8(qry);
			//	printf("query=%s minlen=%d\n", qry, minlen);
			}
			int sl = strlen_utf8(qry);
			if (sl < minlen) {
				fprintf(stderr, "strlen_utf8(query=\"%s\")==%d  minlen=%d\n", qry, sl, minlen);
				return 1;
			}
			for (int i = 0; i < sl; ) {
				int ch_cnt = 0;
				for (int j = i; j < sl; ) {
					j += char_len(qry[j]);
					if (++ch_cnt >= minlen) {
					//	printf("+word: %.*s\n", j-i, qry+i);
						builder.add_word(fstring(qry+i, j-i));
					}
				}
				i += char_len(qry[i]);
			}
		}
		builder.set_word_id_as_lex_ordinal(&offsets, NULL);
		builder.compile();
		builder.sort_by_word_len(offsets);
	}
	const char* MARK_BEG = "\033[1;31m";
	const char* MARK_END = "\033[0m";
	{
		struct stat st;
		::fstat(STDOUT_FILENO, &st);
		if (!S_ISCHR(st.st_mode))
			MARK_BEG = MARK_END = "";
	}
	terark::LineBuf line;
	std::string buf;
	std::string low;
	for (int fidx = 0, fnum = fnames.size(); fidx < fnum; ++fidx) {
		const char* fname = fnames[fidx].c_str();
		terark::Auto_fclose fp(fopen(fname, "r"));
		if (NULL == fp) {
			fprintf(stderr, "ERROR: fopen(\"%s\", \"r\") = %s\n", fname, strerror(errno));
			continue;
		}
		int lineno = 0;
		while (line.getline(fp) >= 0) {
			lineno++;
			line.chomp();
			if (line.size() == 0) continue;
			int  last_end = 0;
			buf.resize(0);
			iterator_to_byte_stream<const char*> input(line.begin(), line.end());
			if (ignore_case) {
				low.assign(line.p, line.size());
				strlower(low);
				input = iterator_to_byte_stream<const char*>(low.data(), low.data()+low.size());
			}
			ac.scan_stream(input, [&](int endpos, const unsigned* hits, int n, size_t){
				int wid = hits[0];
				int hitlen = offsets[wid+1] - offsets[wid];
				int begpos = endpos - hitlen;
				if (last_end < begpos) { // no overlapp
					if (last_end)
						buf += MARK_END;
					buf.append(line.p + last_end, begpos-last_end);
					buf.append(MARK_BEG);
					buf.append(line.p + begpos, hitlen);
				} else {
					if (0 == last_end)
						buf += MARK_BEG;
					buf.append(line.p + last_end, endpos-last_end);
				}
				last_end = endpos;
			});
			if (last_end)
				printf("%s:%d:%s%s%s\n", fname, lineno, buf.c_str(), MARK_END, line.p + last_end);
		}
	}
	return 0;
}

/*
字背景颜色范围:40----49
	 40:黑 41:深红 42:绿 43:黄色 44:蓝色 45:紫色 46:深绿 47:白色

字颜色:30-----------39
	 30:黑 31:红 32:绿 33:黄 34:蓝色 35:紫色 36:深绿 37:白色

===============================================

　　ANSI控制码的说明

===============================================

\33[0m 关闭所有属性
\33[1m 设置高亮度
\33[4m 下划线
\33[5m 闪烁
\33[7m 反显
\33[8m 消隐
\33[30m -- \33[37m 设置前景色
\33[40m -- \33[47m 设置背景色
\33[nA 光标上移n行
\33[nB 光标下移n行
\33[nC 光标右移n行
\33[nD 光标左移n行
\33[y;xH设置光标位置
\33[2J 清屏
\33[K 清除从光标到行尾的内容
\33[s 保存光标位置
\33[u 恢复光标位置
\33[?25l 隐藏光标
\33[?25h 显示光标
*/

#define _SCL_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#define NO_FCGI_DEFINES
#include <terark/fsa/pinyin_dfa.hpp>
#include <terark/fsa/fsa.hpp>
#include <terark/fstring.hpp>
#include <terark/util/linebuf.hpp>
#include <terark/util/profiling.hpp>
#include <terark/util/unicode_iterator.hpp>
#include <terark/lcast.hpp>
#include <terark/zsrch/url.hpp>

#if defined(_MSC_VER)
	int FCGI_Accept_cnt = 1;
	#define FCGI_printf printf
	#define FCGI_Accept() FCGI_Accept_cnt--
	#define FCGI_stdout stdout
	#define FCGI_fwrite fwrite
	#define popen _popen
	#define pclose _pclose
#else
	#define NO_FCGI_DEFINES
	#include <fcgi_stdio.h>
#endif

using namespace terark;

fstrvec sfound;

void OnMatch(size_t, size_t idx, fstring value) {
	TERARK_UNUSED_VAR(idx);
//	FCGI_printf("%d\t%.*s<br/>\n", klen, value.ilen(), value.data());
#if !defined(NDEBUG)
	FCGI_printf("OnMatch: %zd\t%.*s<br/>\n", idx, value.ilen(), value.data());
#endif
//	FCGI_printf("%s<br/>\n", value.str().c_str());
	sfound.push_back(value);
}

int main(int, char* argv[]) {
#if defined(_MSC_VER)
	const char* dfafile = "pinyin-words.dfa.top8m.new";
	const char* basepinyin = "basepinyin.txt";
#else
	const char* dfafile = "/var/dfa-files/pinyin-words.dfa";
	const char* basepinyin = "/var/dfa-files/basepinyin.txt";
#endif
	std::unique_ptr<MatchingDFA> dfa(MatchingDFA::load_from(dfafile));
	fprintf(stderr, "%s: loaded dfa: %s\n", argv[0], dfafile);
	PinYinDFA_Builder pinyin_dfa_builder(basepinyin);
	fprintf(stderr, "%s: built pinyin_dfa_builder: %s\n", argv[0], basepinyin);
	terark::profiling pf;
	size_t count = 0;
	UrlQueryParser urlqry;
	fstrvec hzpinyin, hzpinyin2;
	hash_strmap<> is_touched;
	std::string encode_buf;
	while (FCGI_Accept() >= 0) {
		count++;
//------------------------------------------------------------------
		FCGI_printf("%s",
"Content-type: text/html\r\n\r\n"
R"XXX(<html>
<title>MatchPinyin use FastCGI</title>
<style>
table, th, td {
	border: 1px solid rgba(0, 0, 0, 0.1);
}
table {
	font-size: 0.8em;
	border-collapse: separate;
	border-spacing: 0;
	border-width: 1px 1px 1px 1px;
	margin-bottom: 24px;
}
</style>
<body>
)XXX"
);
//------------------------------------------------------------------
		char* params = getenv("QUERY_STRING");
//		FCGI_printf("query: %s<br/>\n", params);
//		FCGI_printf("pid=%d<br/>\n", getpid());
		if (!params) {
			FCGI_printf("missing query word, by url param 'q'\n");
			continue;
		}
		is_touched.erase_all();
		urlqry.parse(params);
		fstring word = urlqry.get_uniq("q");
		size_t minJianPin = (urlqry.get_uniq("jianpin") == "on") ? 5 : 10000;
		bool verbose = urlqry.get_uniq("verbose") == "1";
		if (verbose) {
			FCGI_printf("<h1>MatchPinyin use FastCGI</h1>");
			FCGI_printf("<p>Request number %zd running on host <i>%s</i><br/></p>\n",
				count, getenv("SERVER_NAME"));
			FCGI_printf("<p>input=%.*s</p>\n", word.ilen(), word.p);
		}
		bool is_hanzi_pinyin_mix = !is_all_hanzi(word);
		size_t hz_cnt = hanzi_count(word);
		long long t2 = pf.now();
		std::unique_ptr<MatchingDFA> pinyin_dfa(pinyin_dfa_builder.make_pinyin_dfa(word, &hzpinyin, minJianPin));
		long long t3 = pf.now();
		if (pinyin_dfa.get() != NULL) {
			sfound.erase_all();
			dfa->match_dfakey_l('\t', *pinyin_dfa, &OnMatch);
			long long t4 = pf.now();
			if (sfound.size()) {
				FCGI_printf("<table><tbody align='center' valign='top'>\n");
				FCGI_printf("<tr bgcolor='LightGoldenRodYellow'>\n");
				for(size_t j = 0; j < word.size(); ) {
					size_t hb = hanzi_bytes(word, j);
					FCGI_printf("<th>%.*s</th>", int(hb), word.p+j);
					j += hb;
				}
				url_encode(word, &encode_buf);
				FCGI_printf("<th></th>"
					"<th rowspan='2' style='line-height:1.4em;'>"
						"<a href='http://www.baidu.com/s?ie=utf-8&wd=%s'>搜百度</a><br/>"
						"<a href='http://www.so.com/s?ie=utf-8&q=%s'>搜360</a>"
					"</th>"
					"<th rowspan='2' style='line-height:1.4em;'>"
						"<a href='https://www.google.com.hk/search?q=%s&ie=UTF-8'>搜谷歌</a><br/>"
						"<a href='http://cn.bing.com/search?go=提交&qs=n&q=%s'>搜bing</a>"
					"</th>"
					, encode_buf.c_str()
					, encode_buf.c_str()
					, encode_buf.c_str()
					, encode_buf.c_str()
					);
				FCGI_printf("</tr>\n<tr bgcolor='LightGoldenRodYellow'>\n");
				for(size_t j = 0; j < hzpinyin.size(); ++j) {
					FCGI_printf("<th align='left'><font color='red'>\n");
					fstring my_duoyin = hzpinyin[j];
					const char* curr = my_duoyin.begin();
					while (curr < my_duoyin.end()) {
						const char* tab = std::find(curr, my_duoyin.end(), '\t');
						FCGI_printf("%.*s%s", int(tab-curr), curr,
								tab < my_duoyin.end()-1 ? "<br/>" : "");
						curr = tab + 1;
					}
					FCGI_printf("</font></th>\n");
				}
				FCGI_printf("<th></th>\n");
				FCGI_printf("</tr>\n");
				FCGI_printf("<tr><td height='6px' colspan='%zd'></td></tr>\n", hzpinyin.size()+3);
				FCGI_printf("<tr bgcolor='LightSkyBlue'>"
							"<th colspan='%zd'>拼音匹配的结果</th><th>词频</th>"
							"<th colspan='3'></th>"
						"</tr>\n", hzpinyin.size());
				FCGI_printf("<tr><td height='4px' colspan='%zd'></td></tr>\n", hzpinyin.size()+3);
				for (size_t i = 0; i < sfound.size(); ++i) {
					fstring s = sfound[i];
					fstring k(s), v("");
					const char* tab = (char*)memchr(s.p, '\t', s.n);
					if (tab) {
						k = fstring(s.p, tab);
						v = fstring(tab+1, s.end());
					}
					pinyin_dfa_builder.get_hzpinyin(k, &hzpinyin2);
					auto ib = is_touched.insert_i(k);
					if (ib.second) {
						if (k == word)
							FCGI_printf("<tr bgcolor='LightGoldenRodYellow'>\n");
						else
							FCGI_printf("<tr>\n");
					}
					else if (!verbose)
						continue;
					else
						FCGI_printf("<tr bgcolor='grey'>\n");
					url_encode(k, &encode_buf);
					if (hanzi_count(k) == hz_cnt) {
						for(size_t j = 0; j < k.size(); ) {
							size_t hb = hanzi_bytes(k, j);
							FCGI_printf("<td>%.*s</td>", int(hb), k.p+j);
							j += hb;
						}
				FCGI_printf("<td rowspan='2' align='right'>%.*s</td>\n"
					"<td rowspan='2' style='line-height:1.4em;'>"
						"<a href='http://www.baidu.com/s?ie=utf-8&wd=%s'>搜百度</a><br/>"
						"<a href='http://www.so.com/s?ie=utf-8&q=%s'>搜360</a>"
					"</td>"
					"<td rowspan='2' style='line-height:1.4em;'>"
						"<a href='https://www.google.com.hk/search?q=%s&ie=UTF-8'>搜谷歌</a><br/>"
						"<a href='http://cn.bing.com/search?go=提交&qs=n&q=%s'>搜bing</a>"
					"</td>"
					, v.ilen(), v.data()
					, encode_buf.c_str()
					, encode_buf.c_str()
					, encode_buf.c_str()
					, encode_buf.c_str()
						);
						if (ib.second) {
							if (k == word)
								FCGI_printf("</tr>\n<tr bgcolor='LightGoldenRodYellow'>\n");
							else
								FCGI_printf("</tr>\n<tr>\n");
						}
						else
							FCGI_printf("</tr>\n<tr bgcolor='grey'>\n");
						for(size_t j = 0; j < hzpinyin2.size(); ++j) {
							FCGI_printf("<td align='left'>\n");
							fstring my_duoyin = hzpinyin2[j];
							const char* curr = my_duoyin.begin();
							fstring src = hzpinyin[j];
						   	while (curr < my_duoyin.end()) {
								tab = std::find(curr, my_duoyin.end(), '\t');
								size_t len = tab - curr;
								const char* x = terark_fstrstr(src.p, src.n, curr, len);
								if (x && (src.begin() == x || '\t' == x[-1]) &&
									   (src.end() == x+len || '\t' == x[len])) {
									FCGI_printf("<font color='red'>%.*s</font>%s", int(tab-curr), curr,
									   	tab < my_duoyin.end()-1 ? "<br/>" : "");
								} else {
									FCGI_printf("%.*s%s", int(tab-curr), curr,
									   	tab < my_duoyin.end()-1 ? "<br/>" : "");
								}
								curr = tab + 1;
							}
							FCGI_printf("</td>\n");
						}
					}
					else if (is_hanzi_pinyin_mix) {
						FCGI_printf("<td colspan='%zd'>%.*s</td>\n", hz_cnt, k.ilen(), k.p);
				FCGI_printf("<td align='right'>%.*s</td>\n"
					"<td rowspan='2' style='line-height:1.4em;'>"
						"<a href='http://www.baidu.com/s?ie=utf-8&wd=%s'>搜百度</a><br/>"
						"<a href='http://www.so.com/s?ie=utf-8&q=%s'>搜360</a>"
					"</td>"
					"<td rowspan='2' style='line-height:1.4em;'>"
						"<a href='https://www.google.com.hk/search?q=%s&ie=UTF-8'>搜谷歌</a><br/>"
						"<a href='http://cn.bing.com/search?go=提交&qs=n&q=%s'>搜bing</a>"
					"</td>"
					, v.ilen(), v.data()
					, encode_buf.c_str()
					, encode_buf.c_str()
					, encode_buf.c_str()
					, encode_buf.c_str()
						);
					}
					else {
						FCGI_printf("<td bgcolor='red' colspan='%zd'>%.*s</td>\n", hz_cnt, k.ilen(), k.p);
						FCGI_printf("<td bgcolor='red'>%.*s</td>\n", v.ilen(), v.p);
						FCGI_printf("<th bgcolor='red' colspan='2'>仅匹配了前缀</th>\n");
					}
					FCGI_printf("</tr>\n");
					if (i < sfound.size()-1)
						FCGI_printf("<tr><td height='4px' colspan='%zd'></td></tr>\n", hzpinyin.size()+3);
				}
				FCGI_printf("</tbody></table>\n");
			}
			else {
			//	FCGI_printf("<h2>Not Found Any Match</h2>");
				FCGI_printf("<h2>未找到匹配的拼音</h2>");
			}
			long long t5 = pf.now();
			FCGI_printf(R"XXX(
			<table><tbody>
				<tr>
					<th align='left' rowspan='3'>耗时</th>
					<td>创建作为 key 的 dfa</td>
					<th align='right'><font color='red'>%8.3f</font> 微秒</th>
				</tr>
				<tr>
					<td>搜索 dfakey</td>
					<th align='right'><font color='red'>%8.3f</font> 微秒%s</th>
				</tr>
				<tr>
					<td>生成网页</td>
					<th align='right'><font color='red'>%8.3f</font> 微秒</th>
				</tr>
			</tbody></table>
			)XXX"
			, pf.uf(t2,t3)
			, pf.uf(t3,t4), (pf.uf(t3,t4) < 5000)?"":"，搜索时间过长，<br>可能是因为PageFault，重试可恢复正常"
			, pf.uf(t4,t5));
		} else {
			FCGI_printf("make_pinyin_dfa failed: <b>%.*s</b><br/>\n", word.ilen(), word.p);
		}
		FCGI_printf("</body></html>\n");
		FCGI_Finish();
	}
	return 0;
}


#define _SCL_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#include <terark/fsa/fsa.hpp>
#include <terark/fsa/dense_dfa.hpp>
#include <terark/fsa/create_regex_dfa.hpp>
#include <terark/util/linebuf.hpp>
#include <terark/util/profiling.hpp>
#include <terark/util/unicode_iterator.hpp>
#include <terark/lcast.hpp>
#include <terark/io/MemStream.hpp>
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

#if !defined(_MSC_VER) && 0
	#define NO_THREADS
	#define create_regex_dfa my_create_regex_dfa
	#include <terark/fsa/create_regex_dfa_impl.hpp>
#endif

using namespace terark;

int num_matched = 0;
AutoGrownMemIO outbuf;

terark::profiling pf;
long long time_output_begin;

template<class DfaKey>
size_t dfa_prefix_match_dfakey_l
(const MatchingDFA* self, size_t root, auchar_t delim, const DfaKey& dfakey, const AcyclicPathDFA::OnMatchKey& on_match)
{
	assert(dfakey.num_zpath_states() == 0);
	if (dfakey.num_zpath_states() > 0) {
		THROW_STD(invalid_argument
			, "dfakey.num_zpath_states()=%lld, which must be zero"
			, (long long)dfakey.num_zpath_states()
			);
	}
	MatchContext ctx;
	valvec<byte_t> strpool;
	gold_hash_map<uint32_t, uint32_t> zcache;
	valvec<MatchContextBase> found;
	terark::AutoFree<CharTarget<size_t> >
		children2(self->get_sigma());

	gold_hash_tab<DFA_Key, DFA_Key, DFA_Key::HashEqual> htab;
	htab.reserve(1024);
	htab.insert_i(DFA_Key{ initial_state, root, 0 });
	size_t nestLevel = self->zp_nest_level();
	size_t depth = 0;
	for(size_t bfshead = 0; bfshead < htab.end_i();) {
		size_t curr_size = htab.end_i();
	//	fprintf(stderr, "depth=%ld bfshead=%ld curr_size=%ld\n", (long)depth, (long)bfshead, (long)curr_size);
		while (bfshead < curr_size) {
			DFA_Key dk = htab.elem_at(bfshead);
			if (dfakey.is_term(dk.s1)) {
				found.push_back({ depth, dk.s2, dk.zidx });
			}
			if (self->v_is_pzip(dk.s2)) {
				fstring zstr;
				if (nestLevel) {
					auto ib = zcache.insert_i(uint32_t(dk.s2), strpool.size());
					if (ib.second) {
						zstr = self->v_get_zpath_data(dk.s2, &ctx);
						strpool.append(zstr.begin(), zstr.size());
					}
					else {
						auto zoff = zcache.val(ib.first);
						auto zbeg = strpool.begin() + zoff;
						if (zcache.end_i() - 1 == ib.first) {
							zstr = fstring(zbeg, strpool.end());
						}
						else {
							zstr = fstring(zbeg, zcache.val(ib.first + 1) - zoff);
						}
					}
				}
				else {
					zstr = self->v_get_zpath_data(dk.s2, &ctx);
				}
			//	fprintf(stderr, "bfshead=%ld zstr.len=%d zidx=%ld\n", (long)bfshead, zstr.ilen(), (long)dk.zidx);
				if (dk.zidx < zstr.size()) {
					byte_t c2 = zstr[dk.zidx];
					if (c2 != delim) {
						size_t t1 = dfakey.state_move(dk.s1, c2);
						if (DfaKey::nil_state != t1)
							htab.insert_i(DFA_Key{ t1, dk.s2, dk.zidx + 1 });
					}
				}
				else goto L1;
			}
			else {
			L1:
				size_t n2 = self->get_all_move(dk.s2, children2);
				for(size_t i = 0; i < n2; ++i) {
					size_t c2 = children2.p[i].ch;
					if (delim != c2) {
						size_t t1 = dfakey.state_move(dk.s1, c2);
						if (t1 != DfaKey::nil_state)
							htab.insert_i(DFA_Key{ t1, children2.p[i].target, 0 });
					}
				}
			}
			bfshead++;
		}
		depth++;
	}
//	fprintf(stderr, "match done, found.size=%ld\n", (long)found.size());
	time_output_begin = pf.now();
	for (size_t i = found.size(); i > 0; ) {
		MatchContextBase x = found[--i];
	//	fprintf(stderr, "prepend_for_each_value(pos=%ld root=%ld zidx=%ld)\n", (long)x.pos, (long)x.root, (long)x.zidx);
		self->prepend_for_each_value(x.pos, 0, "", x.root, x.zidx, on_match);
	//	if (0 == i || found[i-1].pos != x.pos)
	//		break;
	}
	return depth;
}

void OnMatch(int, int idx, fstring value) {
	num_matched++;
	TERARK_UNUSED_VAR(idx);
#if !defined(NDEBUG)
	FCGI_printf("OnMatch: %d\t%.*s<br/>\n", idx, value.ilen(), value.data());
#endif
	const char* tab = value.strstr("\t");
	// source_key in database is tail
	fstring source_key("\tsource:");
	const char* source = value.strstr(source_key);
	if (tab) {
		if (value.data() == tab) {
			outbuf.printf("<tr><td>source:<br/>%.*s</td><td>%.*s</td></tr>\n"
				, int(value.end() - source - source_key.ilen())
				, source + source_key.size()
				, int(source - tab - 1), tab + 1
				);
		}
		else {
			outbuf.printf("<tr><td>source:<br/>%.*s</td><td><font color='grey'>%.*s</font><br/>%.*s</td></tr>\n"
				, int(value.end() - source - source_key.ilen())
				, source + source_key.size()
				, int(tab - value.data()), value.data()
				, int(source - tab - 1), tab + 1
				);
		}
	}
	else {
		// | source | raw_content |
		outbuf.printf("<tr><td>source:<br/>%.*s</td><td>%.*s</td></tr>\n"
			, int(value.end() - source - source_key.ilen())
			, source + source_key.size()
			, int(source - value.begin()), value.begin()
			);
	}
}

int main(int, char* argv[]) {
#if defined(_MSC_VER)
	const char* dfafile = "log-db.udfa";
#else
	setenv("DFA_MAP_LOCKED", "1", true);
	setenv("DFA_MAP_POPULATE", "1", true);
	const char* dfafile = "/var/dfa-files/log-db.dfa";
#endif
	std::unique_ptr<MatchingDFA> dfa(MatchingDFA::load_from(dfafile));
	fprintf(stderr, "%s: loaded dfa: %s\n", argv[0], dfafile);
	size_t count = 0;
	UrlQueryParser urlqry;
	std::string encode_buf;
	while (FCGI_Accept() >= 0) {
		count++;
//------------------------------------------------------------------
		FCGI_printf("%s",
"Content-type: text/html\r\n\r\n"
R"XXX(<html>
<title>LogSearch use FastCGI</title>
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
b {
	color: red
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
		urlqry.parse(params);
		fstring query = urlqry.get_uniq("q"); // q=field:regex
		FCGI_printf("<p>query=%.*s</p>\n", query.ilen(), query.data());
		const char* colon = query.strstr(":");
		if (!query.empty() && colon) {
			try {
				long long t0 = pf.now();
				std::unique_ptr<BaseDFA> regex_dfa(create_regex_dfa(query, ""));
			//	long long t1 = pf.now();
				num_matched = 0;
				outbuf.rewind();
				if (auto rdfa = dynamic_cast<const DenseDFA_uint32_320*>(regex_dfa.get())) {
					dfa_prefix_match_dfakey_l(&*dfa, 0, '\t', *rdfa, &OnMatch);
				}
				else {
					FCGI_printf("<p><b>regex_dfa is not DenseDFA_uint32_320</b></p>\n");
				}
				long long t2 = pf.now();
				if (0 == num_matched) {
					FCGI_printf("<p><b>Not Found Any Result</b></p>\n");
				}
				else {
					FCGI_printf("<p>matched lines: <b>%d</b><br/>time: search: <b>%f</b> seconds, "
								"extract and format result: <b>%f</b> seconds</p>\n"
							, num_matched
							, pf.sf(t0, time_output_begin), pf.sf(time_output_begin, t2)
							);
					AutoGrownMemIO grep_cmd, grep_result;
					if (query.startsWith("title:")) {
						grep_cmd.printf("egrep -c \"title:\\(%.*s.*\\)\" /var/dfa-files/raw-log.txt"
								, int(query.end() - colon - 1), colon + 1
								);
					}
					else if (query.startsWith("datetime:")) {
						grep_cmd.printf("egrep -c \"^%.*s\" /var/dfa-files/raw-log.txt"
								, int(query.end() - colon - 1), colon + 1
								);
					}
					else {
						grep_cmd.printf("egrep -c \"\\<%.*s=%.*s\" /var/dfa-files/raw-log.txt"
								, int(colon - query.begin()), query.p
								, int(query.end() - colon - 1), colon + 1
								);
					}
					fprintf(stderr, "grep_cmd: %s\n", grep_cmd.begin());
					FILE* grep_fp = popen((char*)grep_cmd.begin(), "r");
					if (grep_fp) {
						LineBuf gline;
						while (gline.getline(grep_fp) > 0) {
							gline.chomp();
							grep_result.ensureWrite(gline.p, gline.n);
						}
						pclose(grep_fp);
					}
					long long t3 = pf.now();
					FCGI_printf("<p><strong>grep</strong> matched lines: <b>%.*s</b>, Note: grep can not support multiple fields query<br/>\n", int(grep_result.tell()), grep_result.begin());
					FCGI_printf("<strong>grep</strong> time: <b>%f</b> seconds</p>\n", pf.sf(t2,t3));
					FCGI_printf("<p>our <strong>search speed</strong> is <b>%f</b> times faster than <strong>grep</strong></p>\n", double(t3-t2)/(time_output_begin-t0));
					FCGI_printf("<p>Sending html table time: <span id='SendingTableTime'><b>please wait...</b></span></p>\n");
					FCGI_printf("<table><tbody>\n");
					FCGI_fwrite(outbuf.begin(), 1, outbuf.tell(), FCGI_stdout);
					FCGI_printf("</tbody></table>\n");
					long long t4 = pf.now();
					FCGI_printf("<script language='JavaScript'>"
							"document.getElementById('SendingTableTime').innerHTML = '<b>%f</b> seconds';"
							"</script>\n", pf.sf(t3, t4));
				}
			}
			catch (const std::exception& ex) {
				FCGI_printf("Compile Regex Failed: %s\n", ex.what());
			}
		}
		else {
			FCGI_printf("QUERY_STRING=%s, param 'q' is empty\n", params);
		}
		FCGI_printf("</body></html>\n");
		FCGI_Finish();
	}
	return 0;
}


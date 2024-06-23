#include "pinyin2word.hpp"
#include <stdio.h>
#include <terark/fsa/aho_corasick.hpp>
#include <terark/fsa/iterator_to_byte_stream.hpp>
#include <terark/io/DataIO.hpp>
#include <terark/io/FileStream.hpp>
#include <terark/io/StreamBuffer.hpp>
#include <terark/io/IStream.hpp>
#include <terark/io/IOException.hpp>
#include <terark/io/DataIO_Exception.hpp>
#include <terark/util/autoclose.hpp>
#include <terark/util/linebuf.hpp>

using namespace terark;
typedef Aho_Corasick<Automata<AC_State<State5B> > > base_ac_t;
class TransformerDecoder::ac_t : public base_ac_t {};

TransformerDecoder::TransformerDecoder() {
	dict1 = new ac_t;
	dict2 = new ac_t;
}

TransformerDecoder::~TransformerDecoder() {
	delete dict1;
	delete dict2;
}

void TransformerDecoder::swap(TransformerDecoder& y) {
	std::swap(dict1, y.dict1);
	std::swap(dict2, y.dict2);
	strpool1.swap(y.strpool1);
	strpool2.swap(y.strpool2);
	offsets1.swap(y.offsets1);
	offsets2.swap(y.offsets2);
	map1to2.swap(y.map1to2); map1to2_pool.swap(y.map1to2_pool);
	map2to1.swap(y.map2to1); map2to1_pool.swap(y.map2to1_pool);
}

DATA_IO_LOAD_SAVE_EV(TransformerDecoder, 1,
	   	& (base_ac_t&)(*dict1)
		& (base_ac_t&)(*dict2)
		& strpool1
	   	& strpool2
		& offsets1
	   	& offsets2
		& map1to2
	   	& map1to2_pool
		& map2to1
		& map2to1_pool
		)

bool TransformerDecoder::load(const char* fpath) {
	valvec<valvec<uint> > dyn_map2to1, dyn_map1to2;
	terark::Auto_fclose fp(fopen(fpath, "r"));
	if (NULL == fp) {
		fprintf(stderr, "fopen(\"%s\", \"r\") = %s\n", fpath, strerror(errno));
		return false;
	}
	ac_t::ac_builder builder1(dict1);
	ac_t::ac_builder builder2(dict2);
	terark::LineBuf line;
	while (line.getline(fp) >= 0) {
		line.chomp();
		if (line.size() == 0) continue;
	//	std::transform(line, line+len2, line, &::tolower);
		char* tab = strchr(line, '\t');
		if (NULL == tab) continue;
		if (line == tab) continue; // word1 is empty
		*tab = 0;
		char* w2_beg = tab + 1;
		char* w2_end = std::remove(w2_beg, line.end(), '|'); *w2_end = 0;
		if (w2_beg == w2_end) continue; // word2 is empty
	//	printf("pinyin: %s\n", w2_beg);
		auto wib0 = builder1.add_word(fstring(line, tab));
		if (wib0.second) { // new word
			dyn_map1to2.push_back();
		}
		auto pyib0 = builder2.add_word(fstring(w2_beg, w2_end));
		if (pyib0.second) { // new PinYin
			dyn_map2to1.push_back();
		}
		assert(pyib0.first < dyn_map2to1.size());
		assert( wib0.first < dyn_map1to2.size());
		dyn_map2to1[pyib0.first].push_back( wib0.first);
		dyn_map1to2[ wib0.first].push_back(pyib0.first);
	}
//	printf("dyn_map2to1.size=%zd\n", dyn_map2to1.size());
	if (dyn_map2to1.empty()) {
		fprintf(stderr, "file: %s has no any ChineseWord \\t PinYin\n", fpath);
		return false;
	}
	for (size_t i = 0; i < dyn_map2to1.size(); ++i) {
		valvec<uint>& v = dyn_map2to1[i];
		v.resize(std::unique(v.begin(), v.end()) - v.begin());
	}
	for (size_t i = 0; i < dyn_map1to2.size(); ++i) {
		valvec<uint>& v = dyn_map1to2[i];
		v.resize(std::unique(v.begin(), v.end()) - v.begin());
	}
	valvec<uint> PinYin_old_to_new;	PinYin_old_to_new.resize_no_init(dict2->num_words());
	valvec<uint>   Word_old_to_new;   Word_old_to_new.resize_no_init(dict1->num_words());
	map2to1 .reserve(dict2->num_words()+1);
	offsets2.reserve(dict2->num_words()+1);
	strpool2.reserve(builder2.all_words_size);
	map1to2 .reserve(dict1->num_words()+1);
	offsets1.reserve(dict1->num_words()+1);
	strpool1.reserve(builder1.all_words_size);
	builder1.set_word_id_as_lex_ordinal(
		[&](size_t new_word_id, size_t old_word_id, fstring word) {
			const valvec<uint>& pylist = dyn_map1to2[old_word_id]; // PinYin list of the word
			map1to2.unchecked_push_back(map1to2_pool.size());
			offsets1.unchecked_push_back(strpool1.size());
			map1to2_pool.append(pylist); // old PinYin id list, need to be remappedvoid
			strpool1.append(word);
			Word_old_to_new[old_word_id] = new_word_id;
		});
	map1to2.unchecked_push_back(map1to2_pool.size());
	offsets1.unchecked_push_back(strpool1.size());
	builder1.compile();
	builder1.sort_by_word_len(offsets1);
	builder2.set_word_id_as_lex_ordinal(
		[&](size_t new_py_id, size_t old_py_id, fstring pinyin) {
			const valvec<uint>& wordlist = dyn_map2to1[old_py_id]; // word list of the PinYin
			map2to1.unchecked_push_back(map2to1_pool.size());
			offsets2.unchecked_push_back(strpool2.size());
			map2to1_pool.append(wordlist); // old word id list, need to be remapped
			strpool2.append(pinyin);
			PinYin_old_to_new[old_py_id] = new_py_id;
		});
	map2to1.unchecked_push_back(map2to1_pool.size());
	offsets2.unchecked_push_back(strpool2.size());
	builder2.compile();
	builder2.sort_by_word_len(offsets2);
	for (size_t i = 0; i < map1to2_pool.size(); ++i) {
		uint& py_id = map1to2_pool[i];
		py_id = PinYin_old_to_new[py_id];
	}
	for (size_t i = 0; i < map2to1_pool.size(); ++i) {
		uint& word_id = map2to1_pool[i];
		word_id = Word_old_to_new[word_id];
	}
	return true;
}

std::pair<const uint*, const uint*>
TransformerDecoder::find_x(const fstring str, const ac_t& dict
						 , const valvec<uint>& offsets
						 , const valvec<uint>& map
						 , const valvec<uint>& mappool)
{
	uint wid = dict.find(str, offsets.data());
	if (dict.null_word == wid)
		return std::make_pair((uint*)NULL, (uint*)NULL);
	uint idx0 = map[wid+0];
	uint idx1 = map[wid+1];
	return std::make_pair(mappool.data() + idx0, mappool.data() + idx1);
}
std::pair<const uint*, const uint*>
TransformerDecoder::find_1to2(const fstring str) const {
	return find_x(str, *dict1, offsets1, map1to2, map1to2_pool);
}
std::pair<const uint*, const uint*>
TransformerDecoder::find_2to1(const fstring str) const {
	return find_x(str, *dict2, offsets2, map2to1, map2to1_pool);
}

static void TransformerDecoder_usage(const char* prog) {
	fprintf(stderr, "usage: %s -p PinYinFile (-q PinYinToBeQueried | -w WordFileToBeExpanded)\n", prog);
}

bool TransformerDecoder::load_binary(const char* fname) {
	try {
		FileStream file(fname, "rb");
		file.disbuf();
		NativeDataInput<InputBuffer> dio;
		dio.attach(&file);
		TransformerDecoder tmp;
		dio >> tmp;
		this->swap(tmp);
	}
	catch (const std::exception& ex) {
		fprintf(stderr, "failed: %s:%d, at %s, errno=%s\n", __FILE__, __LINE__, BOOST_CURRENT_FUNCTION, strerror(errno));
		return false;
	}
	return true;
}

int TransformerDecoder::main(int argc, char* argv[]) {
	if (argc < 2) {
		TransformerDecoder_usage(argv[0]);
		return 3;
	}
	const char* wordfile = NULL;
	const char* pinyinfile = NULL;
	const char* str_word = NULL;
	const char* str_pinyin = NULL;
	const char* str_bin_file = NULL;
	bool b_expand_all = false;
	bool load_bin = false;
	for (;;) {
		int ret = getopt(argc, argv, "ap:s:w:q:B:P:W:");
		switch (ret) {
		default:
			fprintf(stderr, "getopt return=%d\n", ret);
			break;
		case -1:
			goto GetoptDone;
		case '?':
			fprintf(stderr, "option -%c requires argument\n", optopt);
			break;
		case 'a':
			b_expand_all = true;
			break;
		case 'p': // PinYinFile
			pinyinfile = optarg;
			break;
		case 'P':
			str_pinyin = optarg;
			break;
		case 's':
			str_bin_file = optarg;
			break;
		case 'B':
			str_bin_file = optarg;
			load_bin = true;
			break;
		case 'W':
			str_word = optarg;
			break;
		case 'w': // word file
			wordfile = optarg;
			break;
		}
	}
GetoptDone:
	if (NULL == pinyinfile && !load_bin) {
		fprintf(stderr, "option \33[1;31m-p pinyin_file\33[0m is required\n");
		return 3;
	}
	if (load_bin)
		load_binary(str_bin_file);
	else
		load(pinyinfile);

	if (str_bin_file) {
		FileStream file(str_bin_file, "wb");
		file.disbuf();
		NativeDataOutput<OutputBuffer> out;
	   	out.attach(&file);
		out << *this;
		out.flush();
	}

	if (wordfile) {
		expand_new_words_pinyin(wordfile);
	}
	if (NULL == str_pinyin && NULL == str_word) {
		return 0;
	}
	uint w2_cnt = 0;
	auto on_word1 = [ ](const char* , int ) {};
	auto on_word2 = [&](const char* word2, int wlen2) {
		w2_cnt++;
		printf("%.*s\n", wlen2, word2);
	};
	const char* src;
	std::unique_ptr<TranverseCTX> tranv;
	if (str_pinyin) {
		src = str_pinyin;
		tranv.reset(make_2to1(on_word1, on_word2));
	} else {
		src = str_word;
		tranv.reset(make_1to2(on_word1, on_word2));
	}
	if (b_expand_all)
		tranv->scan_expand(src);
	else
		tranv->scan_bfs_shortest(src);

	if (0 == w2_cnt) {
		printf("not found valid match for: %s\n", src);
	}
	return 0;
}

///////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////
TransformerDecoder::TranverseCTX::TranverseCTX() {
	// just for Decompose compilation dependency
}

TransformerDecoder::TranverseCTX::~TranverseCTX() {
	// just for Decompose compilation dependency
}

void TransformerDecoder::TranverseCTX::scan_build(const fstring str) {
	for (size_t i = 0; i < forward1.size(); ++i) {
		forward1[i].resize0();
	}
	forward1.resize(str.size());
	iterator_to_byte_stream<const char*> input(str.begin(), str.end());
	dict->scan_stream(input, [&](size_t endpos, const uint* hits, size_t num, size_t) {
		Edge edge;
		for (size_t i = 0; i < num; ++i) {
			uint kid = hits[i];
			uint off0 = offsets1[kid+0];
			uint off1 = offsets1[kid+1];
			uint klen = off1 - off0;
			assert(off1 > off0);
			edge.len = klen;
			edge.kid = kid;
		//	printf("4Word:%.*s\n", off1 - off0, &strpool1[off0]);
			forward1[endpos-klen].push_back(edge);
		}
	});
}

void TransformerDecoder::TranverseCTX::scan_expand(const fstring str) {
	scan_build(str);
	path1.resize(str.size());
	loop1(0);
}

void TransformerDecoder::TranverseCTX::scan_bfs_shortest(const fstring str) {
	scan_build(str);
	path1.resize(str.size()+1);
	valvec<char> mark(str.size()+1);
	valvec<uint> backlink(str.size()+1);
	std::deque<uint> queue;
	queue.push_back(0);
	mark[0] = 1;
	while (!queue.empty()) {
		uint parent = queue.front();
		queue.pop_front();
		if (parent == str.size()) {
			// found a BFS shortest path
			goto Found;
		}
		const valvec<Edge>& edges = forward1[parent];
		for (size_t i = 0; i < edges.size(); ++i) {
			uint child = parent + edges[i].len;
			if (0 == mark[child]) {
			//	printf("parent=%d child=%d\n", parent, child);
				mark[child] = 1;
				queue.push_back(child);
				backlink[child] = parent;
				path1[child] = i;
			}
			else if (1 == mark[child]) {
				// is in queue
			}
			else if (2 == mark[child]) {
				// has been processed
			}
		}
		mark[parent] = 2; // mark processed
	}
//	printf("\nbfs_shortsize_t: dead path, str=%.*s", int(len), str);
	return;
Found:
	path2.resize(0);
	path2.resize(path1.size());
	size_t child = str.size();
	while (child > 0) {
		size_t parent = backlink[child];
		path2[parent] = path1[child];
		child = parent;
	}
	assert(0 == child);
	path1.swap(path2);
	discover1();
}

void TransformerDecoder::TranverseCTX::discover1() {
	forward2.resize(0);
	word1.resize(0);
	size_t pos1 = 0;
	while (pos1 < forward1.size()) {
		if (forward1[pos1].empty()) {
		//	printf("dead path, no match, pos1=%zd\n", pos1);
			return;
		}
		const Edge edge = forward1[pos1][path1[pos1]];
		uint off0 = offsets1[edge.kid+0];
		uint off1 = offsets1[edge.kid+1];
		uint jbeg = map[edge.kid+0]; // 1 to many range
		uint jend = map[edge.kid+1];
		assert(off0 < off1);
		assert(jbeg < jend);
		word1.append(&strpool1[off0], off1 - off0);
		if (-1 != output_sep_ch1)
			word1.push_back((char)output_sep_ch1);
		forward2.push_back();
		for (size_t j = jbeg; j < jend; ++j) {
			uint wid = pool[j];
			forward2.back().push_back(wid);
		}
		pos1 += edge.len;
	}
	assert(pos1 == forward1.size());
	on_word1(word1.c_str(), word1.size());
//		printf("word1=%s\n", word1.c_str());
	path2.resize(forward2.size());
	loop2(0);
}

void TransformerDecoder::TranverseCTX::loop1(size_t pos1) {
	if (forward1.size() == pos1) {
		// found a valid seq1 segmentation
		discover1();
		return;
	}
//	const char* leading = "----------------------------------------------------------------------------------------";
	const valvec<Edge>& edges = forward1[pos1];
//	printf("%.*spos1=%zd  edges:%zd\n", int(pos1), leading, pos1, edges.size());
	for (path1[pos1] = 0; path1[pos1] < edges.size(); ++path1[pos1]) {
		Edge n1 = forward1[pos1][path1[pos1]];
//		uint off0 = offsets1[n1.kid+0];
//		uint off1 = offsets1[n1.kid+1];
//		printf("%.*s%.*s\n", int(pos1), leading, off1-off0, &strpool1[off0]);
		loop1(pos1 + n1.len);
	}
//	printf("\n");
}
void TransformerDecoder::TranverseCTX::loop2(size_t pos2) {
	if (forward2.size() == pos2) {
		// found a seq2 segmentation
		word2.resize(0);
		for (size_t i = 0; i < path2.size(); ++i) {
			uint wid  = forward2[i][path2[i]];
			uint off0 = offsets2[wid+0];
			uint off1 = offsets2[wid+1];
			word2.append(&strpool2[off0], off1 - off0);
			if (-1 != output_sep_ch2)
				word2.push_back((char)output_sep_ch2);
		}
		on_word2(word2.c_str(), word2.size());
		return;
	}
	const valvec<uint>& words = forward2[pos2];
	for (path2[pos2] = 0; path2[pos2] < words.size(); ++path2[pos2]) {
		loop2(pos2 + 1);
	}
}

void TransformerDecoder::expand_new_words_pinyin(fstring wordfile) {
	std::string expanded_file = wordfile + ".pinyin";
	terark::Auto_fclose rfp(fopen(wordfile.c_str(), "r"));
	if (NULL == rfp) {
		fprintf(stderr, "fopen(\"%s\", \"r\") = %s\n", wordfile.c_str(), strerror(errno));
		return;
	}
	terark::Auto_fclose efp(fopen(expanded_file.c_str(), "w"));
	if (NULL == efp) {
		fprintf(stderr, "fopen(\"%s\", \"w\") = %s\n", expanded_file.c_str(), strerror(errno));
		return;
	}
	const char* w1 = NULL;
	int n1 = 0;
	int parts = 0;
	std::auto_ptr<TranverseCTX> tranv(make_1to2(
		[&](const char* word1, int wlen1) {
			w1 = word1; n1 = wlen1;
			parts = std::count(word1, word1+wlen1, '|');
		//	printf("\nWord: %.*s", wlen1, word1);
		//	fprintf(efp, "\n%.*s", wlen1, word1);
		},
		[&](const char* word2, int wlen2) {
		//	printf("\t:%.*s", wlen2, word2);
			if (parts > 1)
				fprintf(efp, "%.*s\t:%.*s\n", n1, w1, wlen2, word2);
		}
	));
	terark::LineBuf line;
	while (line.getline(rfp) >= 0) {
		line.chomp();
		if (line.size() == 0) continue;
	//	printf("3Word: %s\n", line);
		tranv->scan_bfs_shortest(fstring(line.begin(), line.size()));
	}
}

#ifdef TRANSFORMER_DECODER_MAIN
#include <terark/num_to_str.hpp>
int main(int argc, char* argv[]) {
	return TransformerDecoder().main(argc, argv);
}
#endif


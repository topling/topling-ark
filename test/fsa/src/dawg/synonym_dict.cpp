#include "synonym_dict.hpp"
#include <terark/fsa/onfly_minimize.hpp>
#include <terark/io/DataIO.hpp>
#include <terark/io/FileStream.hpp>
#include <terark/io/StreamBuffer.hpp>
#include <terark/util/autofree.hpp>
#include <terark/util/autoclose.hpp>
#include <terark/util/linebuf.hpp>
#include <terark/util/unicode_iterator.hpp>
#include <boost/range/algorithm.hpp>
/*
#if defined(_DEBUG) || !defined(NDEBUG)
	#pragma message "_DEBUG is defined"
#endif
#ifdef DEBUG
	#pragma message "DEBUG is defined"
#endif
#ifdef NDEBUG
	#pragma message "NDEBUG is defined"
#endif
*/
namespace terark {

static size_t g_warn_syno_grp_size = 40;

DATA_IO_LOAD_SAVE_EV(SynonymDict, 1,
		&m_index
		&m_strpool
		&m_offsets
		&m_setlist
		)

using namespace terark;

SynonymDict::SynonymDict() {
	if (char* env = getenv("libmark_warn_syno_grp_size")) {
		g_warn_syno_grp_size = atoi(env);
	}
}

SynonymDict::~SynonymDict() {
	// do nothing...
}

void SynonymDict::swap(SynonymDict& y) {
	m_index  .swap(y.m_index  );
	m_strpool.swap(y.m_strpool);
	m_offsets.swap(y.m_offsets);
	m_setlist.swap(y.m_setlist);
}

void SynonymDict::load_text(char delim, const char* fname) {
	terark::Auto_fclose f(fopen(fname, "r"));
	if (!f) {
		terark::AutoFree<char> msg;
		int len = asprintf(&msg.p, "error: open synonym file: %s", fname);
		fprintf(stderr, "%.*s\n", len, msg.p);
		throw  std::logic_error(msg.p);
	}
	load_text(delim, f);
}

void SynonymDict::load_text(char delim, FILE* f) {
	MinADFA_OnflyBuilder<syno_index_t> builder(&m_index);
	terark::LineBuf line;
	valvec<std::pair<char*,char*> > F;
	while (line.getline(f) > 0) {
		if (line[0] == '#') continue;
		line.chomp();
		line.split(delim, &F);
		for (size_t i = 0; i < F.size(); ++i) {
			auto word = F[i];
			std::transform(word.first, word.second, word.first, tolower);
			builder.add_word(word);
		}
	}
	builder.close();
	m_index.path_zip("DFS");
	m_index.compile();
	m_offsets.resize(m_index.num_words()+1);
	m_setlist.resize(m_index.num_words(), UINT_MAX);
	::rewind(f);
	valvec<uint> synos;
	valvec<uint> merged;
	while (line.getline(f) > 0) {
		if (line[0] == '#') continue;
		line.chomp();
		line.split(delim, &F);
		if (F.empty()) continue;
		synos.resize(0);
		for (size_t i = 0; i < F.size(); ++i) {
			auto word = F[i];
			std::transform(word.first, word.second, word.first, tolower);
			size_t word_id  = m_index.index(word);
			assert(word_id != m_index.null_word);
			assert(word_id  < m_setlist.size());
			synos.push_back(word_id);
		}
		synos.trim(boost::unique(boost::sort(synos)).end());
		uint const nil = UINT_MAX;
		merged.resize(0);
		for (uint word_id : synos) {
			if (nil == m_setlist[word_id])
				merged.push_back(word_id);
			else {
			//	fprintf(stderr, "duplicate synonym word:");
				uint id = word_id;
				do {
				//	fprintf(stderr, " %s", m_index.nth_word(id).c_str());
					merged.push_back(id);
					id = m_setlist[id];
				} while (id != word_id);
			//	fprintf(stderr, "\n");
			}
		}
		merged.trim(boost::unique(boost::sort(merged)).end());
		m_setlist[merged.back()] = merged.front();
		for (size_t i = 1; i < merged.size(); ++i)
			m_setlist[merged[i-1]] = merged[i];
		if (merged.size() != synos.size()) {
			size_t head_word_id = merged[0];
			size_t syno_id = head_word_id;
			DEBUG_fprintf(stderr, "merged synonyms:");
			do {
				assert(syno_id < m_setlist.size());
				DEBUG_fprintf(stderr, " %s", m_index.nth_word(syno_id).c_str());
				syno_id = m_setlist[syno_id];
			} while (syno_id != head_word_id);
			DEBUG_fprintf(stderr, "\n");
		}
	}
	m_index.tpl_for_each_word(
		[this](size_t word_id, const fstring word, size_t) {
		//	fprintf(stderr, "word_id=%06zu %s\n", word_id, word.c_str());
			m_offsets[word_id] = m_strpool.size();
			m_strpool.append(word.data(), word.size());
		});
	m_offsets.back() = m_strpool.size();
	m_strpool.shrink_to_fit();
	valvec<uint> cnt(m_setlist.size(), 0);
	for (size_t word_id = 0; word_id < m_setlist.size(); ++word_id) {
		if (cnt[word_id]) continue;
		size_t  syno_id = word_id;
		size_t  syno_num = 0;
		do {
			assert(syno_id  < m_setlist.size());
			assert(syno_num < m_setlist.size());
			syno_num++;
			cnt[syno_id]++;
			syno_id = m_setlist[syno_id];
		} while (syno_id != word_id);
		if (syno_num > g_warn_syno_grp_size) {
			fprintf(stderr, "synonyms_too_large, num%zd:", syno_num);
			do {
				fprintf(stderr, " %s", m_index.nth_word(syno_id).c_str());
				syno_id = m_setlist[syno_id];
			} while (syno_id != word_id);
			fprintf(stderr, "\n");
		}
	}
	for (size_t word_id = 0; word_id < m_setlist.size(); ++word_id) {
		assert(cnt[word_id] == 1);
	}
	fprintf(stderr, "synonym: words=%zd mem_usage[index=%zd setlink=%zd total=%zd]\n"
			, m_setlist.size()
			, m_index.mem_size()
		    , sizeof(uint)*m_setlist.size()
			, m_index.mem_size() + sizeof(uint)*m_setlist.size()
			);
}

void SynonymDict::append_text(char delim, const char* fname) const {
	terark::Auto_fclose f(fopen(fname, "a"));
	if (f)
		save_text(delim, f);
	else
		fprintf(stderr, "failed: fopen(%s, a) = %s\n", fname, strerror(errno));
}
void SynonymDict::save_text(char delim, const char* fname) const {
	terark::Auto_fclose f(fopen(fname, "w"));
	if (f)
		save_text(delim, f);
	else
		fprintf(stderr, "failed: fopen(%s, w) = %s\n", fname, strerror(errno));
}
void SynonymDict::save_text(char delim, FILE* f) const {
	valvec<uint> cnt(m_setlist.size(), 0);
	valvec<char> buf;
	for (size_t word_id = 0; word_id < m_setlist.size(); ++word_id) {
		if (cnt[word_id]) continue;
		buf.resize(0);
		size_t  syno_id = word_id;
		size_t  syno_num = 0;
		do {
			assert(syno_id  < m_setlist.size());
			assert(syno_num < m_setlist.size());
			syno_num++;
			cnt[syno_id]++;
			size_t off0 = m_offsets[syno_id+0];
			size_t off1 = m_offsets[syno_id+1];
			buf.append(m_strpool.data() + off0, off1 - off0);
			buf.push_back(delim);
			syno_id = m_setlist[syno_id];
		} while (syno_id != word_id);
		buf.back() = '\n';
		::fwrite(buf.data(), 1, buf.size(), f);
	}
}

void SynonymDict::del_bad(char delim, FILE* tmpFile) const {
//	size_t nBuf = 0;
//	terark::AutoFree<char> pBuf;
//	terark::Auto_fclose f(::open_memstream(&pBuf.p, &nBuf));
	std::string line;
	febitvec bits(m_setlist.size());
	for(size_t word_id = 0; word_id < m_setlist.size(); ++word_id) {
		if (bits.is1(word_id))
			continue;
		size_t syno_id = word_id;
		size_t syno_num = 0;
		size_t good_num = 0;
		line.resize(0);
		do {
			assert(syno_id  < m_setlist.size());
			assert(syno_num < m_setlist.size());
			syno_num++;
			assert(bits.is0(syno_id));
			bits.set1(syno_id);
			int off0 = m_offsets[syno_id+0];
			int off1 = m_offsets[syno_id+1];
			const byte_t* p = (byte_t*)m_strpool.data() + off0;
			const int n = off1 - off0;
			int n_utf8 = 0;
			int n_uniq = 0;
			int i = 0;
			uint32_t uch = 0;
			while (i < n) {
				int c = terark::utf8_byte_count(p[i]);
			//	uint32_t uch2 = *terark::make_u8_to_u32_iter(p+i);
				uint32_t uch2 = *terark::u8_to_u32_iterator<const byte_t*>(p+i);
				if (uch2 != uch) {
					n_uniq++;
					uch = uch2;
				}
				n_utf8++;
				i += c;
			}
			TERARK_UNUSED_VAR(n_utf8);
			//  i != n implies encountered invalid utf8
			if (i == n && n_utf8 > 1 && (n_uniq > 1 || n_utf8 < 4)) {
				line.append((const char*)p, n);
				line.push_back(delim);
				good_num++;
			} else {
			#ifdef __DEBUG
				fprintf(stderr, "warning: delete synonym: %.*s\n", n, p);
			#endif
			}
			syno_id = m_setlist[syno_id];
		} while (syno_id != word_id);
		if (good_num > 1) {
			line[line.size()-1] = '\n';
			::fwrite(line.data(), 1, line.size(), tmpFile);
		}
	}
}

void SynonymDict::load_binary(const char* fname) {
	SynonymDict tmp;
	FileStream file;
	if (!file.xopen(fname, "rb")) {
		fprintf(stderr, "failed: SynonymDict::load_binary(%s) = %s\n", fname, strerror(errno));
		throw std::logic_error("SynonymDict::load_binary");
	}
	file.disbuf();
	NativeDataInput<InputBuffer> dio;
	dio.attach(&file);
	dio >> tmp;
	this->swap(tmp);
}

void SynonymDict::save_binary(const char* fname) const {
	using namespace terark;
	FileStream file(fname, "wb");
	file.disbuf();
	NativeDataOutput<OutputBuffer> dio;
	dio.attach(&file);
	dio << *this;
}

} // namespace terark

#ifdef SYNONYM_BUILDER

using namespace terark;

int main(int argc, char* argv[]) {
	// lowercase option is  input
	// UPPERcase option is OUTPUT
	const char *txt = NULL, *TXT = NULL;
	const char *bin = NULL, *BIN = NULL;
	char iDelim = ' ';
	char oDelim = ' ';
	for (;;) {
		int opt = getopt(argc, argv, "d:t:b:T:B:");
		switch (opt) {
			case -1:
				goto GetoptDone;
			case 't': txt = optarg; break;
			case 'T': TXT = optarg; break;
			case 'b': bin = optarg; break;
			case 'B': BIN = optarg; break;
			case 'd': iDelim = optarg[0]; break;
			case 'D': oDelim = optarg[0]; break;
		}
	}
GetoptDone:
//	printf("txt=%s TXT=%s bin=%s BIN=%s\n", txt, TXT, bin, BIN);
	SynonymDict dict;
	if (txt && BIN) {
	   	dict.load_text(iDelim, txt);
	   	dict.save_binary(BIN);
	}
	else if (txt && TXT) {
	   	dict.load_text(iDelim, txt);
	   	dict.save_text(oDelim, TXT);
	}
	else if (bin && TXT) {
	   	dict.load_binary(bin);
	   	dict.save_text(oDelim, TXT);
	}
	else if (bin && BIN) {
	   	dict.load_binary(bin);
	   	dict.save_binary(BIN);
	}
	else {
		fprintf(stderr, "usage %s ( -t input_text_file | -b input_binary_file ) ( -T output_binary_file -B output_binary_file )\n", argv[0]);
		return 1;
	}
	return 0;
}
#endif



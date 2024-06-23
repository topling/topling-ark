#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#define _SCL_SECURE_NO_WARNINGS
#endif
#include <terark/fsa/mre_match.hpp>
#include <terark/fsa/nest_louds_trie.hpp>
#include <terark/fsa/nest_trie_dawg.hpp>
//#include <terark/io/MemStream.hpp>
#include <terark/io/FileStream.hpp>
#include <terark/io/StreamBuffer.hpp>
#include <terark/util/autoclose.hpp>
#include <terark/util/profiling.hpp>
#include <terark/util/linebuf.hpp>
#include <terark/util/unicode_iterator.hpp>
#include <terark/util/crc.hpp>
#include <terark/lcast.hpp>
#include <terark/zsrch/document.hpp>
#include <getopt.h>
#include "RegexCate.inl"

using namespace terark;
using namespace terark::zsrch;

void usage(const char* prog) {
fprintf(stderr, R"EOS(
Usage: %s Options
Options:
   -d InvertIndexDirectory
      Output files in InvertIndexDirectory:
        * inputSpec.to.docID.range.txt
        * term.dawg          : dictionary file
        * index.meta         : meta info text file
        * postlist.offset    : array index to invert.data
        * postlist.data      : docID list array
        * source.node.id     : nodeID == valvec<uint32_t>[docID]
        * source.raw.rpt     : raw souce rptrie
          # this should be built by rptrie_build
   -r ParserRegexDFA
      For example: fields.dfa
   -b ParserRegexBinMeta
      For example: fields.binmeta
   -f InputSpec-Text-File
      File format is:
      Source-Host \t Source-PathFile \t Local-PathFile
   -t MaxTrieNum
   -h
      Show this help info
)EOS", prog);
}

NestLoudsTrieConfig conf;
const char* g_regex_dfa_file = NULL;
const char* g_regex_binmeta_file = NULL;
const char* inputSpec = NULL;
std::string invertDir;
terark::fstrvec m_specVec; // parallel: m_docIDtoSrcSpec.size == m_specVec.size+1
terark::profiling pf;

int parseCommandLine(int argc, char* argv[]) {
	conf.initFromEnv();
	for (;;) {
		int opt = getopt(argc, argv, "b:r:d:f:t:h");
		switch (opt) {
		default:
		case '?':
			usage(argv[0]);
			return 1;
		case -1:
			goto GetoptDone;
		case 'b':
			g_regex_binmeta_file = optarg;
			break;
		case 'r':
			g_regex_dfa_file = optarg;
			break;
		case 'd':
			invertDir = optarg;
			break;
		case 'f':
			inputSpec = optarg;
			break;
		case 't':
			conf.nestLevel = atoi(optarg);
			break;
		case 'h':
			usage(argv[0]);
			exit(0);
		}
	}
GetoptDone:
	if (invertDir.empty()) {
		fprintf(stderr, "-d InvertIndexDirectory is missing\n");
		usage(argv[0]);
		return 1;
	}
	if (NULL == inputSpec) {
		fprintf(stderr, "-f InputSpec-Text-File is missing\n");
		usage(argv[0]);
		return 1;
	}
	Auto_fclose inputSpecFp(fopen(inputSpec, "r"));
	if (!inputSpecFp) {
		fprintf(stderr, "FATAL: fopen('%s', 'r') = %s\n", inputSpec, strerror(errno));
		return 3;
	}
	{
		terark::LineBuf line;
		valvec<fstring> F;
		int lineno = 0;
		while (line.getline(inputSpecFp) > 0) {
			lineno++;
			line.chomp();
			line.split('\t', &F);
			if (F.size() != 3) {
				fprintf(stderr, "FATAL: %s:%d bad inputSpec\n", inputSpec, lineno);
				return 3;
			}
			m_specVec.push_back(F[0], '\0');
			m_specVec.push_back(F[1], '\0');
			m_specVec.push_back(F[2], '\0');
		}
	}
	return 0;
}

void dumpFile(const char* fname, const void* data, size_t size) {
	Auto_fclose fp(fopen(fname, "wb"));
	if (!fp) {
		fprintf(stderr, "FATAL: fopen('%s', 'wb') = %s\n", fname, strerror(errno));
		return;
	}
	const char* p = (const char*)data;
	size_t totalWritten = 0;
	while (totalWritten < size) {
		size_t written = fwrite(p + totalWritten, 1, size - totalWritten, fp);
		if (0 == written) {
			fprintf(stderr, "FATAL: fwrite('%s', size=%lu) = %s\n",
				   	fname, long(size - totalWritten), strerror(errno));
			return;
		}
		totalWritten += written;
	}
}
void dumpFile(fstring fname, const void* data, size_t size) {
	dumpFile(fname.c_str(), data, size);
}
template<class Vec>
void dumpFile(fstring fname, const Vec& v) {
	dumpFile(fname, v.data(), sizeof(typename Vec::value_type) * v.size());
}

class InvertIndexBuilder {

std::unique_ptr<MultiRegexSubmatch> m_submatch;

// maybe multiple same field same value for one doc
hash_strmap<hash_strmap<uint32_t> > m_docFields;
hash_strmap<hash_strmap<> > m_fieldsMapAll;

struct PostListEntry {
	uint32_t docID;
//	uint32_t freq;
};
valvec<uint32_t> m_postListOffset; // m_postListOffset.size == dawg.num_words+1
valvec<uint32_t> m_postListOffsetNew;
//valvec<PostListEntry> m_postListData;
valvec<uint32_t> m_postListData;
//valvec<uint32_t> m_docIDtoSrcSpec; // [0]==0, range [i] to [i+1]
uint32_t m_curDocID;
NestLoudsTrieDAWG_SE m_rpTrie;
bool m_profilingParseTime;
long long m_parseTime;

void parseOneDoc(const char* beg, const char* end) {
	const char* pos = beg;
	while (pos < end) {
		int maxMatchLen = m_submatch->match_utf8(fstring(pos, end), gtab_ascii_tolower);
		for(int regex_id : m_submatch->fullmatch_regex()) {
			int nsub = m_submatch->num_submatch(regex_id);
			switch (RegexCate::EnumOfRegexID[regex_id]) {
			case RegexCate::UnamedRegexID:
				break;
		//	case RegexCate::IgnoreForSpeed:
		//		break;
			case RegexCate::KeyValuePair:
				if (nsub >= 3) {
					fstring key = (*m_submatch)(pos, regex_id, 1);
					fstring val = (*m_submatch)(pos, regex_id, 2);
					m_docFields[key][val]++;
				}
				else {
					fprintf(stderr, "FATAL: KeyValuePair, unexpected nsub=%d\n", nsub);
				}
				break;
			case RegexCate::IpAddress:
				m_docFields["ip"][fstring(pos, maxMatchLen)]++;
				break;
			case RegexCate::date_yyyy_m_d_H_M_S:
				{
				/*
				   fstring yy = (*submatch)(line.p, regex_id, 0);
				   fstring mm = (*submatch)(line.p, regex_id, 1);
				   fstring dd = (*submatch)(line.p, regex_id, 2);
				   fstring hh = (*submatch)(line.p, regex_id, 3);
				   fstring MM = (*submatch)(line.p, regex_id, 4);
				   fstring ss = (*submatch)(line.p, regex_id, 5);
				*/
					m_docFields["datetime"][fstring(pos, maxMatchLen)]++;
				}
				break;
			case RegexCate::title:
				if (nsub >= 2) {
					fstring title = (*m_submatch)(pos, regex_id, 1);
					while (title.n && title.end()[-1] != ')') title.n--;
					title.n--;
					m_docFields["title"][title]++;
				}
				else {
					fprintf(stderr, "FATAL: title, unexpected nsub=%d\n", nsub);
				}
				break;
			}
		}
		if (maxMatchLen)
			pos += maxMatchLen;
		else
			pos += terark::utf8_byte_count(*pos);
	}
}


void firstPassOneDoc(const char* beg, const char* end) {
	for(size_t i = 0; i < m_docFields.end_i(); ++i) {
		m_docFields.val(i).erase_all();
	}
	if (m_profilingParseTime) {
		long long t0 = pf.now();
		parseOneDoc(beg, end);
		long long t1 = pf.now();
		m_parseTime += t1 - t0;
	}
	else {
		parseOneDoc(beg, end);
	}
	for(size_t i = 0; i < m_docFields.end_i(); ++i) {
		fstring fieldName = m_docFields.key(i);
		const
		auto& fieldValueSet1 = m_docFields.val(i);
		if (0 == fieldValueSet1.end_i()) {
			continue;
		}
		auto& fieldValueSet2 = m_fieldsMapAll[fieldName];
		for(size_t j = 0; j < fieldValueSet1.end_i(); ++j) {
			fstring fieldValue = fieldValueSet1.key(j);
		//	auto    valueCount = fieldValueSet1.val(j);
		//	fieldValueSet2[fieldValue] += valueCount;
			fieldValueSet2[fieldValue];
		}
	}
}

valvec<char> m_strBuf;
void secondPassOneDoc(CompactDocFields& compactDoc) {
	m_strBuf.erase_all();
	for(size_t i = 0; i < compactDoc.numFields(); ++i) {
		m_strBuf.assign(compactDoc.field(i));
		m_strBuf.push_back(':');
		size_t nameLen = m_strBuf.size();
		size_t beg = compactDoc.termListBeg(i);
		size_t end = compactDoc.termListEnd(i);
		for(size_t j = beg; j < end; ++j) {
			m_strBuf.risk_set_size(nameLen);
			m_strBuf.append(compactDoc.term(j));
			size_t idx = m_rpTrie.index(fstring(m_strBuf));
			assert(idx < m_rpTrie.num_words());
			m_postListOffset[idx+1]++; // second
		}
	}
}

void thirdPassOneDoc(CompactDocFields& compactDoc) {
	m_strBuf.erase_all();
	for(size_t i = 0; i < compactDoc.numFields(); ++i) {
		m_strBuf.assign(compactDoc.field(i));
		m_strBuf.push_back(':');
		size_t nameLen = m_strBuf.size();
		size_t beg = compactDoc.termListBeg(i);
		size_t end = compactDoc.termListEnd(i);
		for(size_t j = beg; j < end; ++j) {
			m_strBuf.risk_set_size(nameLen);
			m_strBuf.append(compactDoc.term(j));
			size_t idx = m_rpTrie.index(fstring(m_strBuf));
			assert(idx < m_rpTrie.num_words());
			m_postListData[m_postListOffsetNew[idx]++] = m_curDocID; // third
		}
	}
}

void buildTrie() {
	SortableStrVec strVec;
	size_t kvCount = 0;
	size_t poolsize = 0;
	std::string fieldsNameFname = invertDir + "/fieldsName.txt";
	Auto_fclose fieldsNameFp(fopen(fieldsNameFname.c_str(), "w"));
	if (!fieldsNameFp) {
		fprintf(stderr, "ERROR: fopen('%s', 'w') = %s\n",
			   	fieldsNameFname.c_str(), strerror(errno));
	}
	for(size_t i = 0; i < m_fieldsMapAll.end_i(); ++i) {
		fstring fieldName = m_fieldsMapAll.key(i);
		auto& fieldValueSet = m_fieldsMapAll.val(i);
		if (fieldsNameFp)
			fprintf(fieldsNameFp, "%s\n", fieldName.c_str());
		for (size_t j = 0; j < fieldValueSet.end_i(); ++j) {
	//		fprintf(stderr, "\t%s\n", fieldValueSet.key_c_str(j));
			poolsize += fieldValueSet.key_len(j);
		}
		poolsize += (fieldName.size() + 1) * fieldValueSet.end_i();
		kvCount += fieldValueSet.end_i();
	}
	strVec.m_index.reserve(kvCount);
	strVec.m_strpool.reserve(poolsize);
	for(size_t i = 0; i < m_fieldsMapAll.end_i(); ++i) {
		auto& fieldValueSet = m_fieldsMapAll.val(i);
		fstring fieldName = m_fieldsMapAll.key(i);
		for (size_t j = 0; j < fieldValueSet.end_i(); ++j) {
			fstring fieldValue = fieldValueSet.key(j);
			strVec.push_back(fieldName);
			strVec.back_append(":");
			strVec.back_append(fieldValue);
		}
	}
	m_fieldsMapAll.clear();
	m_rpTrie.build_from(strVec, conf);
	m_rpTrie.save_mmap((invertDir + "/term.dawg").c_str());
}
public:
int build() {
	m_submatch.reset(new MultiRegexSubmatch(g_regex_dfa_file, g_regex_binmeta_file));
	std::string docIDRangeFname = invertDir + "/inputSpec.to.docID.range.txt";
	terark::Auto_fclose docIDRangeFp(fopen(docIDRangeFname.c_str(), "w"));
	if (!docIDRangeFp) {
		fprintf(stderr, "FATAL: fopen('%s', 'w') = %s\n",
			   	docIDRangeFname.c_str(), strerror(errno));
		return 3;
	}
	FileStream docTmpFile((invertDir + "/docTmpFile.bin").c_str(), "wb+");
	NativeDataInput<InputBuffer> docTmpInput;
	NativeDataOutput<OutputBuffer> docTmpOutput;
	docTmpInput.attach(&docTmpFile);
	docTmpOutput.attach(&docTmpFile);
	CompactDocFields compactDoc;

	if (const char* env = getenv("profilingParseTime")) {
		m_profilingParseTime = atoi(env) != 0;
	}
	fprintf(stderr, "firstPass...\n");
	m_curDocID = 0;
	m_parseTime = 0;
	size_t byteCount = 0;
	long long t0 = pf.now();
	for (size_t i = 0; i < m_specVec.size(); i += 3) {
		const char* srcHost = m_specVec.c_str(i+0);
		const char* srcPath = m_specVec.c_str(i+1);
		const char* inFname = m_specVec.c_str(i+2);
		Auto_fclose fp(fopen(inFname, "r"));
		if (NULL == fp) {
			fprintf(stderr, "FATAL: firstPass: fopen('%s', 'r') = %s\n",
				   	inFname, strerror(errno));
			return 3;
		}
		m_docFields.erase_all();
		terark::LineBuf line;
		while (line.getline(fp) > 0) {
			byteCount += line.size();
			line.chomp();
			std::transform(line.begin(), line.end(),
				   	line.begin(), [](byte_t c){return tolower(c);});
			firstPassOneDoc(line.begin(), line.end());
			compactDoc.fromHash(m_docFields);
			docTmpOutput << compactDoc;
			m_curDocID++;
			if (m_curDocID % TERARK_IF_DEBUG(1000, 10000) == 0) {
				fprintf(stderr
					, "curDocID=%lu fieldsMapAll.size=%ld ThroughPut=%f'MB\n"
					, long(m_curDocID)
					, long(m_fieldsMapAll.size())
					, byteCount / pf.uf(t0, pf.now())
					);
			}
		}
		long upperDocID = m_curDocID;
		fprintf(docIDRangeFp, "%s\t%s\t%s\t%lu\n",
				srcHost, srcPath, inFname, upperDocID);
	}
	size_t totalDocNum = m_curDocID;
	docTmpOutput.flush();
	fflush(docIDRangeFp);
	buildTrie();
	long long t1 = pf.now();
	m_postListOffset.resize(m_rpTrie.num_words() + 1, 0);
	if (m_profilingParseTime) {
		fprintf(stderr, "parse[time: %f's  ThroughPut: %f'MB]\n",
				pf.sf(0, m_parseTime), byteCount/pf.uf(0, m_parseTime));
		fprintf(stderr, "hash_[time: %f's  ThroughPut: %f'MB]\n",
				pf.sf(0, t1-t0-m_parseTime), byteCount/pf.uf(0, t1-t0-m_parseTime));
	}
	fprintf(stderr, "secondPass...\n");
	docTmpFile.rewind();
	long long t2 = pf.now();
	for (m_curDocID = 0; m_curDocID < totalDocNum; ++m_curDocID) {
		if (m_curDocID && m_curDocID % TERARK_IF_DEBUG(1000, 10000) == 0) {
			fprintf(stderr, "curDocID=%lu ThroughPut=%f'MB\n",
				long(m_curDocID), docTmpFile.tell() / pf.uf(t2, pf.now()));
		}
		docTmpInput >> compactDoc;
		if (compactDoc.checkCRC()) {
			secondPassOneDoc(compactDoc);
		}
		else {
			fprintf(stderr, "ERROR: docID=%ld crc check failed\n", long(m_curDocID));
		}
	}
	// now m_postListOffset[i+1] is the doc count of ith term
	// prefix of count sum is offset
	for (size_t i = 1; i < m_postListOffset.size(); ++i) {
		m_postListOffset[i] += m_postListOffset[i-1];
	}

	fprintf(stderr, "thirdPass...\n");
	m_postListOffsetNew = m_postListOffset;
	m_postListData.resize_no_init(m_postListOffset.back());
	docTmpFile.rewind();
	long long t3 = pf.now();
	for (m_curDocID = 0; m_curDocID < totalDocNum; ++m_curDocID) {
		if (m_curDocID && m_curDocID % TERARK_IF_DEBUG(1000, 10000) == 0) {
			fprintf(stderr, "curDocID=%lu ThroughPut=%f'MB\n",
					long(m_curDocID), docTmpFile.tell() / pf.uf(t3, pf.now()));
		}
		docTmpInput >> compactDoc;
		thirdPassOneDoc(compactDoc);
	}

	dumpFile(invertDir + "/postlist.offset", m_postListOffset);
	dumpFile(invertDir + "/postlist.data", m_postListData);

	return 0;
}

};

size_t g_byteCount;
int buildDocTrieDB() {
	fprintf(stderr, "fourthPass(buildDocTrieDB)...\n");
	SortableStrVec docPool;
	long long t0 = pf.now();
	size_t byteCount = 0;
	long docID = 0;
	for (size_t i = 0; i < m_specVec.size(); i += 3) {
	//	const char* srcHost = m_specVec.c_str(i+0);
	//	const char* srcPath = m_specVec.c_str(i+1);
		const char* inFname = m_specVec.c_str(i+2);
		Auto_fclose fp(fopen(inFname, "r"));
		if (NULL == fp) {
			fprintf(stderr,
				"FATAL: fourthPass(buildDocTrieDB): fopen('%s', 'r') = %s\n",
				inFname, strerror(errno));
			return 3;
		}
		terark::LineBuf line;
		while (line.getline(fp) > 0) {
			byteCount += line.size();
			line.chomp();
			docPool.push_back(line);
			if (docID % TERARK_IF_DEBUG(10000, 100000) == 0) {
				fprintf(stderr, "docID=%lu ThroughPut=%f'MB\n",
						long(docID), byteCount / pf.uf(t0, pf.now()));
			}
			docID++;
		}
		fprintf(stderr, "File=[%s] docID=%lu ThroughPut=%f'MB\n",
				inFname, long(docID), byteCount / pf.uf(t0, pf.now()));
	}
	long long t1 = pf.now();
	valvec<uint32_t> docTrieNode(docPool.size(), UINT32_MAX);
	{
		NestLoudsTrie_SE docTrie;
		docTrie.build_strpool(docPool, docTrieNode, conf);
		size_t size = 0;
		AutoFree<byte_t> data((byte_t*)docTrie.save_mmap(&size));
		dumpFile(invertDir + "/source.raw.rpt", data, size);
	}
	dumpFile(invertDir + "/source.node.id", docTrieNode);
	long long t2 = pf.now();
	fprintf(stderr, "buildDocTrieDB done!\n");
	fprintf(stderr
			, "ThroughPut[read=%f'MB/s, build=%f'MB/s, read+build=%f'MB/s]\n"
			, byteCount/pf.uf(t0, t1)
			, byteCount/pf.uf(t1, t2)
			, byteCount/pf.uf(t0, t2)
		   );
	g_byteCount = byteCount;
	return 0;
}

int main(int argc, char* argv[])
try {
	int ret = parseCommandLine(argc, argv);
	long long t0 = pf.now();
	if (0 == ret)
		ret = InvertIndexBuilder().build();
	if (0 == ret)
		ret = buildDocTrieDB();
	if (0 == ret) {
		fprintf(stderr, "All Done!\n");
		fprintf(stderr, "ThroughPut=%f'MB\n", g_byteCount/pf.uf(t0,pf.now()));
	}
	return ret;
}
catch (const std::exception& ex) {
	return 3;
}


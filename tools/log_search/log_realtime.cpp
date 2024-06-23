#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#define _SCL_SECURE_NO_WARNINGS
#endif
#include <terark/io/DataIO.hpp>
#include <terark/io/FileStream.hpp>
#include <terark/io/StreamBuffer.hpp>
#include <terark/fsa/dict_trie.hpp>
#include <terark/util/autoclose.hpp>
#include <terark/util/profiling.hpp>
#include <terark/util/linebuf.hpp>
#include <terark/util/unicode_iterator.hpp>
#include <terark/util/crc.hpp>
#include <terark/lcast.hpp>
#include <terark/zsrch/document.hpp>
#include <terark/zsrch/regex_query.hpp>
#include <terark/zsrch/url.hpp>
#include <terark/zsrch/zsrch.hpp>
#include <getopt.h>
#include <dlfcn.h> //Makefile:LDFLAGS:-ldl
#include "http.cc"
#include "DocParser.hpp"

using namespace terark;
using namespace terark::zsrch;
terark::profiling pf;
typedef void (*parselog_func)(fstring logline, hash_strmap<hash_strmap<uint32_t> >* docFields);

void usage(const char* prog) {
fprintf(stderr, R"EOS(
Usage: %s Options
Options:
   -d InvertIndexDirectory
      Output files in InvertIndexDirectory:
   -p port service port
   -h
      Show this help info
)EOS", prog);
}

template<class DfaKey>
size_t dfa_prefix_match_dfakey_l
(const DictTrie* dbdfa, size_t root, auchar_t delim,
 const DfaKey& dfakey, size_t maxStateNum, valvec<uint32_t>* wordVec)
{
	assert(dfakey.num_zpath_states() == 0);
	if (dfakey.num_zpath_states() > 0) {
		THROW_STD(invalid_argument
			, "dfakey.num_zpath_states()=%lld, which must be zero"
			, (long long)dfakey.num_zpath_states()
			);
	}
	wordVec->resize(0);
	valvec<MatchContextBase> found;
	terark::AutoFree<CharTarget<size_t> > children2(dbdfa->get_sigma());

	gold_hash_tab<DFA_Key, DFA_Key, DFA_Key::HashEqual> htab;
	htab.reserve(1024);
	htab.insert_i(DFA_Key{ initial_state, root, 0 });
	size_t depth = 0;
	for(size_t bfshead = 0; bfshead < htab.end_i();) {
		size_t curr_size = htab.end_i();
	//	fprintf(stderr, "depth=%ld bfshead=%ld curr_size=%ld\n", (long)depth, (long)bfshead, (long)curr_size);
		while (bfshead < curr_size) {
			if (htab.end_i() > maxStateNum) {
				//THROW_STD(logic_error, "ERROR: exceeding maxStateNum=%ld when Search DFA Key", maxStateNum);
				break;
			}
			DFA_Key dk = htab.elem_at(bfshead);
			if (dfakey.is_term(dk.s1)) {
				found.push_back({ depth, dk.s2, dk.zidx });
			}
			size_t n2 = dbdfa->get_all_move(dk.s2, children2);
			for(size_t i = 0; i < n2; ++i) {
				size_t c2 = children2.p[i].ch;
				if (delim != c2) {
					size_t t1 = dfakey.state_move(dk.s1, c2);
					if (t1 != DfaKey::nil_state)
						htab.insert_i(DFA_Key{ t1, children2.p[i].target, 0 });
				}
			}
			bfshead++;
		}
		depth++;
	}
	if (found.empty() && htab.end_i() > maxStateNum) {
		THROW_STD(logic_error, "ERROR: exceeding maxStateNum=%ld when Search DFA Key", maxStateNum);
	}
	auto onWord = [&](size_t/*nth*/, fstring/*suffix*/, size_t state) {
		size_t wordID = dbdfa->get_mapid(state);
		wordVec->push_back(uint32_t(wordID));
	};
	for (size_t i = found.size(); i > 0; ) {
		MatchContextBase x = found[--i];
		dbdfa->for_each_word_ex(x.root, x.zidx, boost::ref(onWord));
	}
	return depth;
}

class LogSource : public HttpRequestHandler {
public:
	std::string m_source; // host:fullpath
	std::string m_host;
	std::string m_path;
	std::string m_out_fname;
	FileStream  m_out_fp;
	NativeDataOutput<OutputBuffer> m_outBuf;
	hash_strmap<hash_strmap<uint32_t> > m_docFields;
	CompactDocFields m_compactDoc;
	long long   m_startTime;
	std::string m_parserName;
	void* m_parseLogDynaLib;
	std::unique_ptr<DocParser> m_parser;

	explicit LogSource(HttpRequest*) {
		m_parseLogDynaLib = NULL;
		m_startTime = 0;
	}
	virtual ~LogSource() {
		if (m_parseLogDynaLib)
			dlclose(m_parseLogDynaLib);
	}

	void onHeaderEnd(HttpRequest*, HttpServer* server) override;
	void onBodyLine(HttpRequest*, HttpServer*, fstring line) override;
	void onPeerClose(HttpRequest* r, HttpServer* server) override;

	struct GetKey {
		const std::string&
		operator()(const LogSource* x) const { return x->m_source; }
	};
};

class LogSearch : public HttpRequestHandler {
public:
	LogSearch(HttpRequest*) {}
	void onHeaderEnd(HttpRequest* r, HttpServer* server) override;
};

class DocTextList {
	struct DocNode {
		unsigned offset;
		unsigned lineno;
		unsigned fileIndex;
	};
	struct DocNode_StrVecOP {
		size_t get(const DocNode& x) const { return x.offset; }
		void   set(DocNode& x, size_t y) const { x.offset = unsigned(y); }
		void   inc(DocNode& x, ptrdiff_t d = 1) const { x.offset += d; }
		DocNode make(size_t y) const { return DocNode{unsigned(y), 0, 0}; }
		static const unsigned maxpool = unsigned(-1);
	};
	basic_fstrvec<char, DocNode, DocNode_StrVecOP> m_docs;
public:
	void addDoc(fstring docText, size_t fileIndex, size_t lineno) {
		m_docs.offsets.back().fileIndex = uint32_t(fileIndex);
		m_docs.offsets.back().lineno = uint32_t(lineno);
		m_docs.push_back(docText);
	}
	fstring getText(size_t docID) const {
		assert(docID < m_docs.size());
		return m_docs[docID];
	}
	size_t getLineNo(size_t docID) const {
		assert(docID < m_docs.size());
		return m_docs.offsets[docID].lineno;
	}
	size_t getFileIndex(size_t docID) const {
		assert(docID < m_docs.size());
		return m_docs.offsets[docID].fileIndex;
	}
	size_t size() const { return m_docs.size(); }
};

class RealtimeIndex : public IndexReader, public HttpServer {
public:
	gold_hash_tab<std::string, LogSource*,
	   	fstring_hash_equal_align, LogSource::GetKey> m_log_source;
	std::string m_binLogDir;
	DictTrie m_termDict;
	hash_strmap<uint32_t> m_fields; // fieldName -> termCount
	CompactDocFields::fstrvec_with_uintVal m_termsText;
	valvec<valvec<uint32_t> > m_postList;
	DocTextList m_alldocText;

	RealtimeIndex();
	~RealtimeIndex();
	int main(int argc, char* argv[]);

// IndexReader Methods:
	void getDocMeta(size_t docID, DocMeta* docMeta) const override;
	void getDocData(size_t docID, std::string* data) const override;
	void getTermList(const Query*, valvec<uint32_t>* termList) const override;
	void getTermString(size_t termID, std::string* termStr) const override;
	void getDocList(const valvec<uint32_t>& termList, valvec<uint32_t>* docList) const override;
	void getDocList(size_t termID, valvec<uint32_t>* docList) const override;
	size_t getDocListLen(size_t termID) const override;
};

RealtimeIndex::RealtimeIndex() {
	m_uriHandlerMap.replace("/add", new DefaultHandlerFactory<LogSource>());
	m_uriHandlerMap.replace("/search", new DefaultHandlerFactory<LogSearch>());
}

RealtimeIndex::~RealtimeIndex() {
}

int RealtimeIndex::main(int argc, char* argv[]) {
	for (;;) {
		int opt = getopt(argc, argv, "d:p:h");
		switch (opt) {
		default:
		case '?':
			usage(argv[0]);
			return 1;
		case -1:
			goto GetoptDone;
		case 'd':
			m_binLogDir = optarg;
			break;
		case 'p':
			m_port = atoi(optarg);
			break;
		case 'h':
			usage(argv[0]);
			exit(0);
		}
	}
GetoptDone:
	if (m_binLogDir.empty()) {
		fprintf(stderr, "ERROR: missing argument -d binLogDirectory\n");
		usage(argv[0]);
		return 1;
	}
	if (0 == m_port) {
		fprintf(stderr, "ERROR: missing argument -p port\n");
		usage(argv[0]);
		return 1;
	}
	return this->start(m_port);
}

void RealtimeIndex::getDocMeta(size_t docID, DocMeta* docMeta) const {
	docMeta->docID = docID;
	docMeta->lineno = m_alldocText.getLineNo(docID);
	docMeta->fileIndex = m_alldocText.getFileIndex(docID);
	docMeta->host = m_log_source.elem_at(docMeta->fileIndex)->m_host;
	docMeta->path = m_log_source.elem_at(docMeta->fileIndex)->m_path;
}

void RealtimeIndex::getDocData(size_t docID, std::string* data) const {
	if (docID >= m_alldocText.size()) {
		THROW_STD(out_of_range, "ERROR: docID=%lu beyond maxDocID=%lu",
			   	long(docID), long(m_alldocText.size()));
	}
	fstring d = m_alldocText.getText(docID);
	data->assign(d.data(), d.size());
}

void RealtimeIndex::getTermList(const Query* qry, valvec<uint32_t>* termList) const {
	const RegexQuery* rq = dynamic_cast<const RegexQuery*>(qry);
	auchar_t delim = 256; // no delim
	size_t maxStateNum = 10000;
	size_t maxDepth = dfa_prefix_match_dfakey_l(&m_termDict
			, initial_state, delim
		   	, *rq->m_qryDFA, maxStateNum, termList);
	(void) maxDepth;
}

void RealtimeIndex::getTermString(size_t termID, std::string* termStr) const {
	termStr->resize(0);
	fprintf(stderr
		, "getTermString: termID=%zd num_words=%zd m_termsText.size=%zd\n"
		, termID, m_termDict.num_words(), m_termsText.size()
		);
	assert(termID < m_termDict.num_words());
	assert(m_termDict.num_words() == m_termsText.size());
	size_t fieldIndex = m_termsText.offsets[termID].uintVal;
	fstring fieldTerm = m_termsText[termID];
	fstring fieldName = m_fields.key(fieldIndex);
	termStr->append(fieldName.data(), fieldName.size());
//	termStr->append(":");
	termStr->append(fieldTerm.data(), fieldTerm.size());
}

void RealtimeIndex::getDocList(const valvec<uint32_t>& termList, valvec<uint32_t>* docList) const {
	size_t totalDocNum = 0;
	for(size_t i = 0; i < termList.size(); ++i) {
		size_t termID = termList[i];
		totalDocNum += m_postList[termID].size();
	}
	docList->resize_no_init(totalDocNum);
	totalDocNum = 0;
	uint32_t* p = docList->data();
	for(size_t i = 0; i < termList.size(); ++i) {
		size_t termID = termList[i];
		const valvec<uint32_t>& list = m_postList[termID];
		std::copy_n(list.data(), list.size(), p + totalDocNum);
		totalDocNum += list.size();
	}
	if (termList.size() > 1) {
		std::sort(p, p + totalDocNum);
		docList->trim(std::unique(p, p + totalDocNum));
		docList->shrink_to_fit();
	}
}

void RealtimeIndex::getDocList(size_t termID, valvec<uint32_t>* docList) const {
	assert(termID < m_postList.size());
	*docList = m_postList[termID];
}

size_t RealtimeIndex::getDocListLen(size_t termID) const {
	assert(termID < m_postList.size());
	return m_postList[termID].size();
}

void LogSource::onHeaderEnd(HttpRequest* r, HttpServer* server) {
	RealtimeIndex* index = (RealtimeIndex*)server;
	fprintf(stderr, "queryString: %s\n", r->queryString().c_str());
	fstring srcHost = r->arg("srcHost");
	fstring srcFile = r->arg("srcFile");
	m_parserName = r->arg("parser").str();
	if (m_parserName.empty()) {
		THROW_STD(invalid_argument,
			"Invalid Request Arguments, parser is required, queryString=%s\n",
			r->queryString().c_str());
	}
	m_source = srcHost + ":" + srcFile;
	m_host.assign(srcHost.begin(), srcHost.size());
	m_path.assign(srcFile.begin(), srcFile.size());
	m_out_fname = srcHost + "-++-" + srcFile;
	std::replace(m_out_fname.begin(), m_out_fname.end(), '/', '_');
	m_out_fname = index->m_binLogDir + "/" + m_out_fname;
	m_out_fp.open(m_out_fname.c_str(), "wb");
	m_outBuf.attach(&m_out_fp);
	Auto_fclose fp(fopen(m_parserName.c_str(), "r"));
	if (!fp) {
		THROW_STD(invalid_argument
			, "FATAL: fopen(%s, r) = %s", m_parserName.c_str(), strerror(errno));
	}
	LineBuf line;
	std::string dynaLib;
	std::string conf;
	while (line.getline(fp) > 0) {
		if (strncmp(line.begin(), "dll=", 4) == 0) {
			line.chomp();
			dynaLib.assign(line.begin() + 4, line.end());
		}
		else {
			conf.append(line.begin(), line.size());
		}
	}
	m_parseLogDynaLib = dlopen(dynaLib.c_str(), RTLD_LAZY|RTLD_LOCAL);
	fprintf(stderr, "srcHost=%.*s\n", srcHost.ilen(), srcHost.data());
	fprintf(stderr, "srcFile=%.*s\n", srcFile.ilen(), srcFile.data());
	fprintf(stderr, "dynaLib=%.*s\n", (int)dynaLib.size(), dynaLib.data());
	if (NULL == m_parseLogDynaLib) {
		THROW_STD(invalid_argument, "FATAL: dlopen: %s\n", dlerror());
	}
	else {
		DocParserFactory createParser = (DocParserFactory)dlsym(m_parseLogDynaLib, "createParser");
		if (NULL == createParser) {
			THROW_STD(invalid_argument
					, "FATAL: dlsym(%s, createParser) = %s\n"
					, dynaLib.c_str(), dlerror());
		}
		m_parser.reset(createParser(conf));
		if (!m_parser) {
			THROW_STD(invalid_argument
					, "FATAL: dynaLib[%s]::createParser(%s) failed\n"
					, dynaLib.c_str(), conf.c_str());
		}
	}
	m_startTime = pf.now();
	if (index->m_log_source.insert_i(this).second) {
		fprintf(stderr, "register %s successed\n", m_source.c_str());
	}
	else {
		fprintf(stderr, "register %s failed, existed\n", m_source.c_str());
	}
}

void LogSource::onBodyLine(HttpRequest* r, HttpServer* server, fstring line) {
	RealtimeIndex* index = (RealtimeIndex*)server;
	size_t fileIdx = index->m_log_source.find_i(m_source);
	assert(fileIdx < index->m_log_source.end_i());
	size_t docID = index->m_alldocText.size();
	line.chomp();
	char* beg = (char*)line.begin();
	char* end = (char*)line.end();
	std::transform(beg, end, beg, [](byte_t c){return tolower(c);});
	for (size_t i = 0; i < m_docFields.end_i(); ++i) {
		m_docFields.val(i).erase_all();
	}
	m_parser->parse(line, &m_docFields);
	std::string fieldName;
	for(size_t i = 0; i < this->m_docFields.end_i(); ++i) {
		if (this->m_docFields.empty())
			continue;
		fieldName = this->m_docFields.key(i).str();
		fieldName.push_back(':');
		auto colonChild = index->m_termDict.trie_add_word(fieldName);
		if (colonChild.second) {
			index->m_termsText.offsets.back().uintVal = UINT32_MAX;
			index->m_termsText.push_back(fieldName);
		}
		size_t fieldIndex = index->m_fields.insert_i(fieldName).first;
		const auto& fieldValueSet = this->m_docFields.val(i);
		for(size_t j = 0; j < fieldValueSet.end_i(); ++j) {
			fstring fieldValue = fieldValueSet.key(j);
		//	auto    valueCount = fieldValueSet1.val(j);
			auto ibTermState = index->m_termDict.trie_add_word(colonChild.first, fieldValue);
			size_t wordid = index->m_termDict.get_mapid(ibTermState.first);
			if (index->m_postList.size() <= wordid) {
				index->m_postList.resize(wordid+1);
			}
			if (ibTermState.second) {
				index->m_fields.val(fieldIndex)++;
				index->m_termsText.offsets.back().uintVal = fieldIndex;
				index->m_termsText.push_back(fieldValue);
			}
			index->m_postList[wordid].push_back(uint32_t(docID));
		}
	}
	index->m_alldocText.addDoc(line, fileIdx, r->bodyLineCount());
	this->m_compactDoc.fromHash(this->m_docFields);
	this->m_outBuf << this->m_compactDoc;
	if (docID % TERARK_IF_DEBUG(1000, 10000) == 0) {
		fprintf(stderr
				, "docID=%lu fields=%ld ThroughPut=%f'MB\n"
				, long(docID), long(this->m_docFields.end_i())
				, r->recvBytes() / pf.uf(this->m_startTime, pf.now())
			   );
	}
}

void LogSource::onPeerClose(HttpRequest* r, HttpServer* server) {
//	RealtimeIndex* index = (RealtimeIndex*)server;
	size_t docNum = r->bodyLineCount();
	fprintf(stderr, "EOF at remote file: %s\n", m_source.c_str());
	fprintf(stderr, "docNum=%lu ThroughPut=%f'MB\n", long(docNum),
			r->recvBytes() / pf.uf(m_startTime, pf.now()));
//	index->m_log_source.erase(m_source);
	r->m_handler.release(); // keep it
}

void LogSearch::onHeaderEnd(HttpRequest* r, HttpServer* server) {
	RealtimeIndex* indexReader = (RealtimeIndex*)server;
	fstring qry0 = r->arg("q");
	int fd = r->fd();
	errno = 0;
	int ret = 0;
/*
	int ret = dprintf(fd,
		"HTTP/1.0 200 OK\r\n"
		"Content-type: text/xml\r\n\r\n"
		"<html>\n"
		"<head>\n"
			"<title>Query Hello World!</title>\n"
		"</head>\n"
		"<body>\n"
		"Query = %.*s\n"
		, qry0.ilen(), qry0.data()
		);
	fprintf(stderr, "ret=%d dprintf() = %s\n", ret, strerror(errno));
*/
	std::string strqry;
	url_decode(qry0, &strqry);
	int bShowAll = false;
	try {
		bShowAll = lcast(r->arg("docData"));
	}
	catch (const std::exception&) {}
	terark::zsrch::QueryPtr qry(Query::createQuery(strqry));
	valvec<uint32_t> termList, docList;
	indexReader->getTermList(qry.get(), &termList);
	dprintf(fd,
		"HTTP/1.0 200 OK\r\n"
		"Content-type: text/xml\r\n\r\n"
	   R"EOS(<?xml version="1.0" encoding="utf-8"?>)EOS""\n"
		"<body>\n"
		"  <query><![CDATA[%s]]></query>\n"
		, strqry.c_str()
		);
	fprintf(stderr, "ret=%d dprintf() = %s\n", ret, strerror(errno));
	if (termList.empty()) {
		dprintf(fd, "  <status>NotFound</status>\n");
	}
	else {
		dprintf(fd, "  <status>OK</status>\n"
					"  <matchedTerms>\n"
				);
		std::string term;
		for (size_t i = 0; i < termList.size(); ++i) {
			long termID = termList[i];
			long docCnt = indexReader->getDocListLen(termID);
			indexReader->getTermString(termID, &term);
			dprintf(fd,
				"    <term>\n"
				"       <id>%ld</id>\n"
				"       <docCount>%ld</docCount>\n"
				"       <str><![CDATA[%s]]></str>\n"
				"    </term>\n"
				, termID, docCnt, term.c_str()
				);
		}
		dprintf(fd, "  </matchedTerms>\n");
		if (bShowAll) {
			indexReader->getDocList(termList, &docList);
			dprintf(fd, "  <docCount>%ld</docCount>\n"
						"  <documents>\n"
					, long(docList.size())
					);
			DocMeta docMeta;
			std::string content;
			for (size_t i = 0; i < docList.size(); ++i) {
				long docID = docList[i];
				size_t lineno = size_t(-1);
				indexReader->getDocMeta(docID, &docMeta);
				indexReader->getDocData(docID, &content);
				dprintf(fd,
					"    <document>\n"
					"      <id>%ld</id>\n"
					"      <host><![CDATA[%s]]></host>\n"
					"      <file><![CDATA[%s]]></file>\n"
					"      <line>%ld</line>\n"
					"      <content><![CDATA[%s]]></content>\n"
					"    </document>\n"
					, docID, docMeta.host.c_str(), docMeta.path.c_str()
					, long(lineno + 1) // 0-based to 1-based
					, content.c_str()
					);
			}
			dprintf(fd, "  </documents>\n");
		}
	}
	dprintf(fd, "</body>\n");

	r->closefd();
}

int main(int argc, char* argv[])
try {
	return RealtimeIndex().main(argc, argv);
}
catch (const std::exception&) {
	return 3;
}


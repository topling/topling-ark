#include "zsrch.hpp"
#include "regex_query.hpp"
#include <terark/fstring.hpp>
#include <terark/hash_strmap.hpp>
#include <terark/util/fstrvec.hpp>
#include <terark/util/linebuf.hpp>
#include <terark/util/mmap.hpp>
#include <terark/util/profiling.hpp>
#include <terark/fsa/dense_dfa.hpp>
#include <terark/fsa/nfa.hpp>
#include <terark/fsa/nest_trie_dawg.hpp>

namespace terark { namespace zsrch {

typedef NestLoudsTrieDAWG_SE_512 TrieDAWG;

/*
static
void* ensureLoad(fstring fname, size_t expectedSize) {
	size_t size = 0;
	void*  base = mmap_load(fname.c_str(), &size);
	if (size != expectedSize) {
		THROW_STD(invalid_argument, "FATAL: bad file: %.*s, size=%llu expectedSize=%llu"
				, fname.ilen(), fname.data()
				, (long long)(size), (long long)(expectedSize));
	}
	return base;
}
*/

template<class T>
void ensureLoad(fstring fname, T** ppArrayBase, size_t expectedArraySize) {
	*ppArrayBase = NULL;
	size_t size = 0;
	void*  base = mmap_load(fname.c_str(), &size);
	if (size != sizeof(T) * expectedArraySize) {
		mmap_close(base, size);
		THROW_STD(invalid_argument
				, "FATAL: bad file: %.*s, size=%llu expected[bytes=%llu size=%llu]"
				, fname.ilen(), fname.data()
			   	, (long long)(size)
				, (long long)(expectedArraySize) * sizeof(T)
				, (long long)(expectedArraySize)
				);
	}
	*ppArrayBase = (T*)base;
}

template<class DfaKey>
size_t dfa_prefix_match_dfakey_l
(const TrieDAWG& dbdfa, size_t root, auchar_t delim,
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
	MatchContext ctx;
	valvec<byte_t> strpool;
	gold_hash_map<uint32_t, uint32_t> zcache;
	valvec<MatchContextBase> found;
	terark::AutoFree<CharTarget<size_t> > children2(dbdfa.get_sigma());

	gold_hash_tab<DFA_Key, DFA_Key, DFA_Key::HashEqual> htab;
	htab.reserve(1024);
	htab.insert_i(DFA_Key{ initial_state, root, 0 });
	size_t nestLevel = dbdfa.zp_nest_level();
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
			if (dbdfa.is_pzip(dk.s2)) {
				fstring zstr;
				if (nestLevel) {
					auto ib = zcache.insert_i(uint32_t(dk.s2), strpool.size());
					if (ib.second) {
						zstr = dbdfa.get_zpath_data(dk.s2, &ctx);
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
					zstr = dbdfa.get_zpath_data(dk.s2, &ctx);
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
				size_t n2 = dbdfa.get_all_move(dk.s2, children2);
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
	if (found.empty() && htab.end_i() > maxStateNum) {
		THROW_STD(logic_error, "ERROR: exceeding maxStateNum=%ld when Search DFA Key", maxStateNum);
	}
	auto onWord = [&](size_t/*nth*/, fstring/*suffix*/, size_t state) {
		size_t wordID = dbdfa.state_to_word_id(state);
		wordVec->push_back(wordID);
	};
	for (size_t i = found.size(); i > 0; ) {
		MatchContextBase x = found[--i];
		dbdfa.for_each_word_ex(x.root, x.zidx, ref(onWord));
	}
	return depth;
}

Query* Query::createQuery(fstring strQuery) {
	return new RegexQuery(strQuery);
}
Query::~Query() {}

IndexReader::~IndexReader() {
}

void IndexReader::getDocList(const Query* qry, valvec<uint32_t>* docList) const {
	valvec<uint32_t> termList;
	getTermList(qry, &termList);
	getDocList(termList, docList);
}

class CompactIndexReader : public IndexReader {
	fstrvec m_specVec;
	std::unique_ptr<TrieDAWG> m_termDict;
	NestLoudsTrie_SE m_docTrieDB;   // source.raw.rpt
	void *  m_docTrieDB_base;   // source.raw.rpt
	size_t  m_docTrieDB_size;   // source.raw.rpt
	valvec<uint32_t>  m_startDocID;
	uint32_t*  m_docTrieTail; // source.node.id
	uint32_t*  m_docListData;
	uint32_t*  m_docListOffset;
public:
	CompactIndexReader(const std::string& indexDir);
	~CompactIndexReader();
	void getDocMeta(size_t docID, DocMeta* docMeta) const;
	void getDocData(size_t docID, std::string* data) const;
	void getTermList(const Query* qry, valvec<uint32_t>* termList) const;
	void getTermString(size_t termID, std::string* term) const;
	void getDocList(const valvec<uint32_t>& termList, valvec<uint32_t>* docList) const;
	void getDocList(size_t termID, valvec<uint32_t>* docList) const;
	size_t getDocListLen(size_t termID) const;
};

CompactIndexReader::CompactIndexReader(const std::string& indexDir) {
	BaseDFA* dfa = BaseDFA::load_from(indexDir + "/term.dawg");
	m_termDict.reset(dynamic_cast<TrieDAWG*>(dfa));
	if (!m_termDict) {
		THROW_STD(invalid_argument, "FATAL: file '%s/term.dawg' is not a DAWG", indexDir.c_str());
	}
	{
		std::string fname = indexDir + "/inputSpec.to.docID.range.txt";
		Auto_fclose fp(fopen(fname.c_str(), "r"));
		if (!fp) {
			THROW_STD(invalid_argument, "FATAL: fopne('%s', 'r') = %s", fname.c_str(), strerror(errno));
		}
		LineBuf line;
		valvec<fstring> F;
		m_startDocID.push_back(0);
		while (line.getline(fp) > 0) {
			line.chomp();
			line.split('\t', &F);
			if (F.size() != 4) {
				THROW_STD(invalid_argument, "FATAL: invalid file %s", fname.c_str());
			}
			m_specVec.push_back(F[0], '\0');
			m_specVec.push_back(F[1], '\0');
			m_specVec.push_back(F[2], '\0'); // local file pathname
			unsigned long upperDocID = strtoul(F[3].data(), NULL, 10);
			m_startDocID.push_back(uint32_t(upperDocID));
		}
	}
	size_t totalDocNum = m_startDocID.back();
	{
		size_t size = 0;
		void * base = mmap_load((indexDir + "/source.raw.rpt").c_str(), &size);
		m_docTrieDB_base = base;
		m_docTrieDB_size = size;
		m_docTrieDB.load_mmap(base, size);
	}
	ensureLoad(indexDir + "/source.node.id" , &m_docTrieTail  , totalDocNum);
	ensureLoad(indexDir + "/postlist.offset", &m_docListOffset, m_termDict->num_words() + 1);
	ensureLoad(indexDir + "/postlist.data"  , &m_docListData  , m_docListOffset[m_termDict->num_words()]);
}

CompactIndexReader::~CompactIndexReader() {
	size_t totalDocNum = m_startDocID.back();
	size_t docListOffsetSize = m_termDict->num_words() + 1;
	size_t docListDataSize = m_docListOffset[docListOffsetSize - 1];
	mmap_close(m_docTrieTail   , sizeof(uint32_t) * totalDocNum);
	mmap_close(m_docListData   , sizeof(uint32_t) * docListDataSize);
	mmap_close(m_docListOffset , sizeof(uint32_t) * docListOffsetSize);
	m_docTrieDB.risk_release_ownership();
	mmap_close(m_docTrieDB_base, m_docTrieDB_size);
}

void CompactIndexReader::getDocMeta(size_t docID, DocMeta* docMeta) const {
	auto upper = std::upper_bound(m_startDocID.begin(), m_startDocID.end(), uint32_t(docID));
	if (upper == m_startDocID.end()) {
		THROW_STD(out_of_range, "ERROR: docID=%lu beyond maxDocID=%lu",
			   	long(docID), long(m_startDocID.back()));
	}
	long idx = upper - m_startDocID.begin();
	docMeta->lineno = docID - upper[-1];
	docMeta->docID = docID;
	docMeta->host = m_specVec.beg_of(3*idx + 0);
	docMeta->path = m_specVec.beg_of(3*idx + 1);
	docMeta->fileIndex = size_t(idx);
}

void CompactIndexReader::getDocData(size_t docID, std::string* data) const {
	if (docID >= m_startDocID.back()) {
		THROW_STD(out_of_range, "ERROR: docID=%lu beyond maxDocID=%lu",
			   	long(docID), long(m_startDocID.back()));
	}
	size_t node = m_docTrieTail[docID];
	m_docTrieDB.restore_string(node, data);
}

void CompactIndexReader::getTermList(const Query* qry, valvec<uint32_t>* termList) const {
	const RegexQuery* rq = dynamic_cast<const RegexQuery*>(qry);
	auchar_t delim = 256; // no delim
	size_t maxStateNum = 10000;
	size_t maxDepth = dfa_prefix_match_dfakey_l(*m_termDict
			, initial_state, delim
		   	, *rq->m_qryDFA, maxStateNum, termList);
	(void) maxDepth;
}

void CompactIndexReader::getTermString(size_t termID, std::string* term) const {
	if (termID >= m_termDict->num_words()) {
		THROW_STD(out_of_range, "termID=%ld, termDictSize=%ld",
			   	long(termID), long(m_termDict->num_words()));
	}
	m_termDict->nth_word(termID, term);
}

void CompactIndexReader::getDocList(const valvec<uint32_t>& termList, valvec<uint32_t>* docList) const {
	size_t totalDocNum = 0;
	for(size_t i = 0; i < termList.size(); ++i) {
		size_t termID = termList[i];
		size_t off0 = m_docListOffset[termID + 0];
		size_t off1 = m_docListOffset[termID + 1];
		totalDocNum += off1 - off0;
	}
	docList->resize_no_init(totalDocNum);
	totalDocNum = 0;
	uint32_t* p = docList->data();
	for(size_t i = 0; i < termList.size(); ++i) {
		size_t termID = termList[i];
		size_t off0 = m_docListOffset[termID + 0];
		size_t off1 = m_docListOffset[termID + 1];
		size_t size = off1 - off0;
		std::copy_n(m_docListData + off0, size, p + totalDocNum);
		totalDocNum += size;
	}
	if (termList.size() > 1) {
		std::sort(p, p + totalDocNum);
		docList->trim(std::unique(p, p + totalDocNum));
		docList->shrink_to_fit();
	}
}

void CompactIndexReader::getDocList(size_t termID, valvec<uint32_t>* docList) const {
	if (termID >= m_termDict->num_words()) {
		THROW_STD(out_of_range, "termID=%ld >= termDictSize=%ld",
				long(termID), long(m_termDict->num_words()));
	}
	size_t off0 = m_docListOffset[termID+0];
	size_t off1 = m_docListOffset[termID+1];
	size_t size = off1 - off0;
	docList->resize_no_init(size);
	std::copy_n(m_docListData + off0, size, docList->data());
}

size_t CompactIndexReader::getDocListLen(size_t termID) const {
	if (termID >= m_termDict->num_words()) {
		THROW_STD(out_of_range, "termID=%ld >= termDictSize=%ld",
				long(termID), long(m_termDict->num_words()));
	}
	size_t off0 = m_docListOffset[termID+0];
	size_t off1 = m_docListOffset[termID+1];
	size_t size = off1 - off0;
	return size;
}

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

IndexReader* IndexReader::loadIndex(const std::string& indexDir) {
	return new CompactIndexReader(indexDir);
}

}} // namespace terark::zsrch

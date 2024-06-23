// terark_btree_impl.cpp

/**
 *    Copyright (C) 2014 MongoDB Inc.
 *
 *    This program is free software: you can redistribute it and/or  modify
 *    it under the terms of the GNU Affero General Public License, version 3,
 *    as published by the Free Software Foundation.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU Affero General Public License for more details.
 *
 *    You should have received a copy of the GNU Affero General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *    As a special exception, the copyright holders give permission to link the
 *    code of portions of this program with the OpenSSL library under certain
 *    conditions as described in each individual source file and distribute
 *    linked combinations including the program with the OpenSSL library. You
 *    must comply with the GNU Affero General Public License in all respects for
 *    all of the code used other than as permitted herein. If you modify file(s)
 *    with this exception, you may extend this exception to your version of the
 *    file(s), but you are not obligated to do so. If you do not wish to do so,
 *    delete this exception statement from your version. If you delete this
 *    exception statement from all source files in the program, then also delete
 *    it in the license file.
 */

#define MONGO_LOG_DEFAULT_COMPONENT ::mongo::logger::LogComponent::kStorage
#include "terark_btree_impl.h"
#include <mongo/util/log.h>
#include <set>

#include "mongo/platform/basic.h"
#include <mongo/db/json.h>
#include "mongo/db/catalog/index_catalog_entry.h"
#include "mongo/db/storage/index_entry_comparison.h"
#include "mongo/stdx/memory.h"
#include "mongo/util/mongoutils/str.h"
#include <mongo/util/hex.h>

#include "terark_recovery_unit.h"
#include <terark/fsa/dawg.hpp>
#include <terark/fsa/onfly_minimize.hpp>
#include <terark/fsa/nest_trie_dawg.hpp>
#include <terark/io/FileStream.hpp>
#include <terark/io/StreamBuffer.hpp>
#include <terark/io/DataIO.hpp>
#include <terark/util/mmap.hpp>

#include "index_key.h"

namespace mongo {

using std::shared_ptr;
using std::string;
using std::vector;

using namespace terark;

class IndexCatalogEntry;

namespace {

const int TempKeyMaxSize = 1024;  // this goes away with SERVER-3372

bool hasFieldNames(const BSONObj& obj) {
    BSONForEach(e, obj) {
        if (e.fieldName()[0])
            return true;
    }
    return false;
}

BSONObj stripFieldNames(const BSONObj& query) {
    if (!hasFieldNames(query))
        return query;

    BSONObjBuilder bb;
    BSONForEach(e, query) {
        bb.appendAs(e, StringData());
    }
    return bb.obj();
}

class IndexChange : public RecoveryUnit::Change {
public:
    virtual void commit() {}
    virtual void rollback() {}
};

class TerarkWriteOnlyIndex;
class TerarkWriteOnlyIndexBuilder : public SortedDataBuilderInterface {
public:
    TerarkWriteOnlyIndexBuilder(TerarkWriteOnlyIndex* woIndex) : m_woIndex(woIndex) {}
    Status addKey(const BSONObj& key, const RecordId& loc);
private:
    TerarkWriteOnlyIndex* m_woIndex;
};

class TerarkWriteOnlyIndex : public SortedDataInterface {
public:
    TerarkWriteOnlyIndex(const boost::filesystem::path& dbpath, fstring ident) {
    	boost::filesystem::path binlogFile = dbpath / (ident + ".binlog");
    	m_fp.open(binlogFile.string().c_str(), "wb+");
    	m_dio.attach(&m_fp);
    	m_entryNum = 0;
        m_dataSize = 0;
    }

    SortedDataBuilderInterface* getBulkBuilder(OperationContext*, bool dupsAllowed)
    override {
        return new TerarkWriteOnlyIndexBuilder(this);
    }

    Status
	insert(OperationContext*, const BSONObj& key, const RecordId& loc, bool dupsAllowed)
    override {
        invariant(loc.isNormal());
        invariant(!hasFieldNames(key));
        if (key.objsize() >= TempKeyMaxSize) {
            string msg = mongoutils::str::stream()
                << "TerarkBtree::insert: key is too large(size=" << key.objsize()
                << "), keydata: " << key;
            return Status(ErrorCodes::KeyTooLong, msg);
        }
        writeEntry(key, loc);
        return Status::OK();
    }

    void unindex(OperationContext*, const BSONObj& key, const RecordId&, bool dupsAllowed)
    override {
    	THROW_STD(invalid_argument, "UnsupportedCommand");
    }

    void fullValidate(OperationContext* txn, bool full, long long* numKeysOut,
                      BSONObjBuilder* output)
    const override {
        // TODO check invariants?
        *numKeysOut = m_entryNum;
    }

    bool appendCustomStats(OperationContext* txn, BSONObjBuilder* output, double scale)
    const override {
        return false;
    }

    long long getSpaceUsedBytes(OperationContext* txn)
    const override {
        return m_dataSize;
    }

    Status dupKeyCheck(OperationContext* txn, const BSONObj& key, const RecordId& loc)
    override {
        return Status::OK();
    }

    virtual bool isEmpty(OperationContext* txn) { return 0 == m_entryNum; }

    virtual Status touch(OperationContext* txn) const { return Status::OK(); }

    class Cursor final : public SortedDataInterface::Cursor {
    public:
        boost::optional<IndexKeyEntry> next(RequestedInfo parts) override { return {}; }
        void setEndPosition(const BSONObj& key, bool inclusive) override { }

        boost::optional<IndexKeyEntry>
        seek(const BSONObj&, bool, RequestedInfo) override { return {}; }

        boost::optional<IndexKeyEntry>
        seek(const IndexSeekPoint&, RequestedInfo) override { return {}; }

        void save() override { }
        void saveUnpositioned() override { }
        void restore() override { }
        void detachFromOperationContext() final { }
        void reattachToOperationContext(OperationContext* txn) final { }
    };

    std::unique_ptr<SortedDataInterface::Cursor>
    newCursor(OperationContext* txn, bool isForward) const override {
        return stdx::make_unique<Cursor>();
    }

    Status initAsEmpty(OperationContext* txn) override { return Status::OK(); }

    void writeEntry(const BSONObj& key, RecordId loc) {
        m_dio << uint32_t(loc.repr());
        if (key.objsize() < 5)
        	log() << "key.objsize()=" << key.objsize();
        m_dio.ensureWrite(key.objdata(), key.objsize());
        m_dataSize += key.objsize() + sizeof(uint32_t);
        m_entryNum++;
    }

    void finishBuild(const boost::filesystem::path& dbPath, fstring ident) {
    	using namespace terark;
    	m_dio.flush();
    	m_fp.rewind();
		NativeDataInput<InputBuffer> dio; dio.attach(&m_fp);
		TerarkBsonBlob encodedKey;
		TerarkBsonBlob entryBuf;
    	DAWG<State32> dawg;
    	typedef Automata<State32> MyDFA;
    	{
        	MyDFA dfa;
			{
            	auto indexJsonFile = dbPath / (ident.str() + ".json");
            	FileStream indexJsonFp(indexJsonFile.string().c_str(), "w");
    			MinADFA_OnflyBuilder<MyDFA> onfly(&dfa);
				for (size_t i = 0; i < m_entryNum; ++i) {
					dio.skip_obj<uint32_t>(); // recId
					dio >> entryBuf;
					BSONObj bsonkey(entryBuf.data());
					terarkEncodeIndexKey(bsonkey, &encodedKey);
					std::string jsonkey = tojson(bsonkey);
					fprintf(indexJsonFp, "%s\n", jsonkey.c_str());
					try {
						onfly.add_word(encodedKey);
					}
					catch (const std::exception& ex) {
						fprintf(stderr, "i=%zd, caught: %s\n", i, ex.what());
						return;
					}
				}
			}
	    	dawg.path_zip(dfa, "DFS");
    	}
    	dawg.compile();
    	auto dawgFile = dbPath / (ident + ".index");
    	dawg.save_mmap(dawgFile.string().c_str());

    	invariant(dio.is_bufeof());
    	dio.resetbuf();
    	m_fp.rewind();
    	valvec<uint32_t> dupidx(dawg.num_words() + 1, 0);
		for (size_t i = 0; i < m_entryNum; ++i) {
			dio.skip_obj<uint32_t>(); // recId
			dio >> entryBuf;
			BSONObj bsonkey(entryBuf.data());
			terarkEncodeIndexKey(bsonkey, &encodedKey);
			size_t index = dawg.index(encodedKey);
			invariant(index < dawg.num_words());
			dupidx[index+1]++;
		}
		// prefix sum
		for(size_t i = 2; i < dupidx.size(); ++i) dupidx[i] += dupidx[i-1];

		rank_select_se_512 bitmap(m_entryNum + 1, false);
		for(size_t i = 0; i < dupidx.size()-1; ++i) {
			size_t j = dupidx[i+0];
			size_t k = dupidx[i+1];
			for(size_t s = j+1; s < k; ++s) bitmap.set1(s);
		}
	//	bitmap.set0(m_entryNum);
		bitmap.build_cache(true, false);
    	valvec<uint32_t> idvec(m_entryNum, valvec_no_init());
    	invariant(dio.is_bufeof());
    	dio.resetbuf();
    	m_fp.rewind();
		for (size_t i = 0; i < m_entryNum; ++i) {
			uint32_t recId = dio.load_as<uint32_t>();
			dio >> entryBuf;
			BSONObj bsonkey(entryBuf.data());
			terarkEncodeIndexKey(bsonkey, &encodedKey);
			size_t index = dawg.index(encodedKey);
			invariant(index < dawg.num_words());
			idvec[dupidx[index]++] = recId;
		}
		// sort record id list for same key
		for(size_t i = 0; i < dawg.num_words(); ++i) {
			size_t k = dupidx[i];
			size_t j = bitmap.select0(i);
			invariant(j + 1 + bitmap.one_seq_len(j+1) == k);
			std::sort(idvec.data() + j, idvec.data() + k);
		}

    	auto recIdFile = dbPath / (ident + ".recid");
    	FileStream fp(recIdFile.string().c_str(), "wb");
    	NativeDataOutput<OutputBuffer> fpdo; fpdo.attach(&fp);
    	fpdo << uint32_t(m_entryNum);
    	fpdo << uint32_t(bitmap.mem_size());
    	fpdo.ensureWrite(idvec.data(), idvec.used_mem_size());
    	fpdo.ensureWrite(bitmap.data(), bitmap.mem_size());
    }

private:
    FileStream m_fp;
    NativeDataOutput<OutputBuffer> m_dio;
    size_t m_entryNum;
    long long m_dataSize;
};
Status TerarkWriteOnlyIndexBuilder::addKey(const BSONObj& key, const RecordId& loc) {
    // inserts should be in ascending (key, RecordId) order.
    if (key.objsize() >= TempKeyMaxSize) {
        return Status(ErrorCodes::KeyTooLong, "key too big");
    }
    invariant(loc.isNormal());
    invariant(!hasFieldNames(key));
    m_woIndex->writeEntry(key, loc);
    return Status::OK();
}

/// Readonly Index

class TerarkReadonlyIndexBuilder : public SortedDataBuilderInterface {
public:
    Status addKey(const BSONObj& key, const RecordId& loc) {
        return Status(ErrorCodes::CommandNotSupported, "TerarkDB is readonly");
    }
};
class TerarkReadonlyIndex : public SortedDataInterface {
public:
	TerarkReadonlyIndex(const boost::filesystem::path& dbPath, fstring ident) {
		auto dawgFile = dbPath / (ident + ".index");
		BaseDFA* dfa = BaseDFA::load_from(dawgFile.string());
		m_dfa.reset(dfa);
		m_dawg = dynamic_cast<BaseDAWG*>(dfa);
		if (!m_dawg) {
			THROW_STD(invalid_argument, "DFA is not a DAWG");
		}
		auto recIdFile = dbPath / (ident + ".recid");
		m_mmapBase = (byte_t*)terark::mmap_load(recIdFile.string(), &m_mmapSize);
		size_t entryNumber = ((uint32_t*)m_mmapBase)[0];
		size_t bitmapBytes = ((uint32_t*)m_mmapBase)[1];
		this->m_locArray.risk_set_data((uint32_t*)m_mmapBase + 2, entryNumber);
		this->m_locBitMap.risk_mmap_from((byte_t*)m_locArray.end(), bitmapBytes);
		log() << BOOST_CURRENT_FUNCTION
				<< ": dawgWordNum=" << m_dawg->num_words()
				<< ", entryNumber=" << entryNumber
				<< ", bitmapBytes=" << bitmapBytes;
    }
	virtual ~TerarkReadonlyIndex() {
		m_locArray.risk_release_ownership();
		m_locBitMap.risk_release_ownership();
		terark::mmap_close(m_mmapBase, m_mmapSize);
	}

    SortedDataBuilderInterface* getBulkBuilder(OperationContext*, bool) override {
        return new TerarkReadonlyIndexBuilder();
    }
    Status insert(OperationContext*, const BSONObj&, const RecordId&, bool)
    override {
    	log() << BOOST_CURRENT_FUNCTION << ": tracing";
        return Status(ErrorCodes::CommandNotSupported, "TerarkDB is readonly");
    }
    void unindex(OperationContext*, const BSONObj&, const RecordId&, bool)
    override {
        THROW_STD(invalid_argument, "TerarkDB is readonly");
    }

    void fullValidate(OperationContext* txn, bool full, long long* numKeysOut,
					  BSONObjBuilder* output)
    const override {
        *numKeysOut = m_locBitMap.max_rank1();
    }

    bool appendCustomStats(OperationContext*, BSONObjBuilder* output, double scale)
    const override {
        return false;
    }

    long long getSpaceUsedBytes(OperationContext*) const override {
        return m_dfa->mem_size() + m_locBitMap.mem_size() + m_locArray.used_mem_size();
    }

    Status dupKeyCheck(OperationContext* txn, const BSONObj& key, const RecordId& loc)
    override {
    	log() << BOOST_CURRENT_FUNCTION << ": tracing";
        return Status(ErrorCodes::CommandNotSupported, "TerarkDB is readonly");
    }

    bool isEmpty(OperationContext* txn) override {
    	invariant(m_dawg->num_words() > 0);
        return m_dawg->num_words() == 0;
    }

    Status touch(OperationContext* txn) const override {
        // already in memory...
        return Status::OK();
    }

    std::unique_ptr<SortedDataInterface::Cursor>
    newCursor(OperationContext* txn, bool isForward) const override;

    Status initAsEmpty(OperationContext* txn) override {
        // No-op
        return Status::OK();
    }

private:
    std::unique_ptr<terark::BaseDFA> m_dfa;
    terark::BaseDAWG*    m_dawg;
    terark::valvec<uint32_t>   m_locArray; // loc is record id to NestLouds StringPool
    terark::rank_select_se_512 m_locBitMap;
    unsigned char* m_mmapBase;
    size_t m_mmapSize;
};

class TerarkReadonlyIndexCursor final : public SortedDataInterface::Cursor {
public:
    TerarkReadonlyIndexCursor(OperationContext* txn
    		, const terark::BaseDAWG* dawg
			, const terark::valvec<uint32_t>* locArray
			, const terark::rank_select_se_512* locBitMap
			, bool isForward
			)
	: _txn(txn)
	, _dfa(dynamic_cast<const BaseDFA*>(dawg))
	, _dawg(dawg)
	, _locBitMap(locBitMap)
	, _locArr(locArray->data())
	, _locIdx(locArray->size())
	, _locLen(locArray->size())
	, _forward(isForward ? true : false)
	{}

    bool isEof() const {
    //	return (_forward ? _locLen : 0) == _locIdx;
    	return _forward * _locLen == _locIdx; // equivalent with above
    }

    size_t locBeg() const { return !_forward * _locLen; }
    size_t locEnd() const { return  _forward * _locLen; }

    boost::optional<IndexKeyEntry> next(RequestedInfo parts) override {
        if (_lastMoveWasRestore) {
            _lastMoveWasRestore = false;
        } else {
            advance();
        }
        if (isEof())
            return {};
        return getEntry(_locIdx, parts);
    }

    void setEndPosition(const BSONObj& key, bool inclusive) override {
        _locIdx = _forward ? _locLen : 0;
    }

    BSONObj getKeyFromLocArrayIndex(size_t locIdx) const {
    	if (_forward) {
    		invariant(locIdx < _locLen);
    	} else {
    		invariant(locIdx > 0);
    		locIdx--;
    	}
        size_t dawgIdx = _locBitMap->rank0(locIdx);
        std::string keyData = _dawg->nth_word(dawgIdx);
        BSONObj bsonkey = terarkDecodeIndexKey(keyData, BSONObj());
    //    std::string jsonkey = tojson(bsonkey);
    //    log() << "getKeyFromLocArrayIndex(" << locIdx << ") = " << jsonkey
    //    		<< ", binhex=" << toHexLower(keyData.data(), keyData.size());
        return bsonkey;
    }
    IndexKeyEntry getEntry(size_t locIdx, RequestedInfo parts) const {
    	if (parts & kWantKey)
    		return IndexKeyEntry(getKeyFromLocArrayIndex(locIdx), RecordId(_locArr[locIdx - !_forward]));
    	else
    		return IndexKeyEntry({}, RecordId(_locArr[locIdx - !_forward]));
    }

    boost::optional<IndexKeyEntry>
    seek(const BSONObj& key, bool inclusive, RequestedInfo parts) override {
        const BSONObj query = stripFieldNames(key);
        log() << BOOST_CURRENT_FUNCTION << ": key=" << tojson(key);
        log() << BOOST_CURRENT_FUNCTION << ": qry=" << tojson(query);
        DawgIndexIter dawgIter = _dawg->dawg_lower_bound(terarkEncodeIndexKey(query));
        if (dawgIter.isFound) {
		//	if (!_forward && inclusive || _forward && !inclusive) dawgIter.index++;
        	if (_forward != inclusive) dawgIter.index++; // equivalent as above
        }
        _locIdx = _locBitMap->select0(dawgIter.index);
        _lastMoveWasRestore = false;
        /*
        log() << BOOST_CURRENT_FUNCTION << ": dawgIter={"
        	<< "index: " << dawgIter.index
			<< ", matchLen: " << dawgIter.matchedLen
			<< ", isFound: " << dawgIter.isFound
			<< "}, locIdx=" << _locIdx
			<< ", inclusive=" << inclusive
			<< ", parts=" << int(parts);
		*/
        if (isEof())
            return {};
        return getEntry(_locIdx, parts);
    }

    boost::optional<IndexKeyEntry>
    seek(const IndexSeekPoint& seekPoint, RequestedInfo parts) override {
        const BSONObj query = IndexEntryComparison::makeQueryObject(seekPoint, _forward);
        DawgIndexIter dawgIter = _dawg->dawg_lower_bound(terarkEncodeIndexKey(query));
        if (!_forward && dawgIter.isFound) {
        	dawgIter.index++;
        }
    //  log() << BOOST_CURRENT_FUNCTION << ": qry=" << tojson(query);
        _locIdx = _locBitMap->select0(dawgIter.index);
        _lastMoveWasRestore = false;
        return getEntry(_locIdx, parts);
    }

    void save() override {
        _txn = nullptr;
        if (_lastMoveWasRestore)
            return;
        _savedLoc = _locIdx;
    }

    void saveUnpositioned() override { }

    void restore() override {
        _lastMoveWasRestore = isEof();
        _locIdx = _savedLoc;
    }

    void detachFromOperationContext() override {
        _txn = nullptr;
    }

    void reattachToOperationContext(OperationContext* txn) override {
        _txn = txn;
    }

private:
    // Advances once in the direction of the scan, updating _isEOF as needed.
    // Does nothing if already _isEOF.
    void advance() {
    	if (_forward) {
    		invariant(_locIdx < _locLen);
    		_locIdx++;
    	} else {
    		invariant(_locIdx > 0);
    		_locIdx--;
    	}
    }

    OperationContext* _txn;  // not owned
    const terark::BaseDFA* _dfa;
    const terark::BaseDAWG* _dawg;
    const terark::rank_select_se_512* _locBitMap;
    const uint32_t* _locArr;
    size_t _locIdx;
    size_t _locLen;
    size_t _savedLoc = 0;

    const bool _forward;

    // Used by next to decide to return current position rather than moving. Should be reset
    // to false by any operation that moves the cursor, other than subsequent save/restore
    // pairs.
    bool _lastMoveWasRestore = false;
};

std::unique_ptr<SortedDataInterface::Cursor>
TerarkReadonlyIndex::newCursor(OperationContext* txn, bool isForward) const {
	log() << BOOST_CURRENT_FUNCTION << ": txn->getNS()=" << txn->getNS();
    return std::unique_ptr<SortedDataInterface::Cursor>(new TerarkReadonlyIndexCursor(
    			txn, m_dawg, &m_locArray, &m_locBitMap, isForward));
}


}  // namespace

// IndexCatalogEntry argument taken by non-const pointer for consistency with other Btree
// factories. We don't actually modify it.
SortedDataInterface*
getTerarkWriteOnlyIndex(const boost::filesystem::path& dbPath, fstring ident) {
    return new TerarkWriteOnlyIndex(dbPath, ident);
}
void finishWriteOnlyIndex(const boost::filesystem::path& dbPath,
						  terark::fstring ident,
						  SortedDataInterface* sdiIndex) {
	invariant(NULL != sdiIndex);
	auto woIndex = dynamic_cast<TerarkWriteOnlyIndex*>(sdiIndex);
	invariant(NULL != woIndex);
	woIndex->finishBuild(dbPath, ident);
}

SortedDataInterface*
getTerarkReadOnlyIndex(const boost::filesystem::path& dbPath, terark::fstring ident) {
	return new TerarkReadonlyIndex(dbPath, ident);
}

}  // namespace mongo



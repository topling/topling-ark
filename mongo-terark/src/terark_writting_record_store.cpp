// terark_record_store.cpp

/**
 *    Copyright (C) 2014 MongoDB Inc.
 *
 *    This program is free software: you can redistribute it and/or  modify
 *    it under the terms of the GNU Affero General Public License, version 3,
 *    as published by the Free Software Foundation.
 *
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

#include "terark_writting_record_store.h"
#include "record_codec.h"
#include <terark/io/FileStream.hpp>

#include "mongo/platform/basic.h"
#include "mongo/bson/json.h"
#include "mongo/db/jsobj.h"
#include "mongo/db/namespace_string.h"
#include "mongo/db/operation_context.h"
#include "mongo/db/storage/oplog_hack.h"
#include "mongo/db/storage/recovery_unit.h"
#include "mongo/stdx/memory.h"
#include "mongo/util/hex.h"
#include "mongo/util/log.h"
#include "mongo/util/mongoutils/str.h"
#include "mongo/util/unowned_ptr.h"

namespace mongo {

using std::shared_ptr;

class TerarkWrittingRecordStore::InsertChange : public RecoveryUnit::Change {
public:
    virtual void commit() {}
    virtual void rollback() {
    	invariant(!"TerarkWrittingRecordStore::InsertChange never rollback");
    }
};

// Works for both removes and updates
class TerarkWrittingRecordStore::RemoveChange : public RecoveryUnit::Change {
public:
    virtual void commit() {
    	invariant(!"TerarkWrittingRecordStore should never RemoveChange");
    }
    virtual void rollback() {
    	invariant(!"TerarkWrittingRecordStore should never rollback");
    }
};

class TerarkWrittingRecordStore::TruncateChange : public RecoveryUnit::Change {
public:
    virtual void commit() {
    	invariant(!"TerarkWrittingRecordStore should never TruncateChange");
    }
    virtual void rollback() {
    	invariant(!"TerarkWrittingRecordStore should never rollback");
    }
};

class TerarkWrittingRecordStore::Cursor final : public SeekableRecordCursor {
public:
    Cursor(const terark::SortableStrVec& records)
        : m_records(records) {}

    boost::optional<Record> next() final {
        if (_needFirstSeek) {
            _needFirstSeek = false;
            m_recId = 0;
        } else if (!_lastMoveWasRestore && m_recId < m_records.size()) {
            m_recId++;
        }
        _lastMoveWasRestore = false;

        if (m_recId == m_records.size())
            return {};
        terark::fstring fdata = m_records[m_recId];
        RecordData data(fdata.data(), fdata.size());
        data.makeOwned();
        return {{RecordId(m_recId+1), data}};
    }

    boost::optional<Record> seekExact(const RecordId& id) final {
        _lastMoveWasRestore = false;
        _needFirstSeek = false;
        m_recId = id.repr() - 1;
        if (m_recId == m_records.size())
            return {};
        terark::fstring fdata = m_records[m_recId];
        RecordData data(fdata.data(), fdata.size());
        data.makeOwned();
        return {{RecordId(m_recId+1), data}};
    }

    void save() final {
        if (!_needFirstSeek && !_lastMoveWasRestore)
            _savedId = m_recId == m_records.size() ? size_t(-1) : m_recId;
    }

    void saveUnpositioned() final {
        _savedId = m_records.size();
    }

    bool restore() final {
        if (_savedId == size_t(-1)) {
            m_recId = m_records.size();
            return true;
        }
        m_recId = _savedId;
        _lastMoveWasRestore = m_recId == m_records.size() || m_recId != _savedId;
        return !(_lastMoveWasRestore);
    }

    void detachFromOperationContext() final {}
    void reattachToOperationContext(OperationContext* txn) final {}

private:
    const terark::SortableStrVec& m_records;
    size_t m_recId = 0;
    size_t _savedId = size_t(-1);
    bool _needFirstSeek = true;
    bool _lastMoveWasRestore = false;
};

// reverse_iterator::operator*() { return *--baseIter; }

class TerarkWrittingRecordStore::ReverseCursor final : public SeekableRecordCursor {
public:
    ReverseCursor(const terark::SortableStrVec& records)
        : m_records(records) {}

    boost::optional<Record> next() final {
        if (_needFirstSeek) {
            _needFirstSeek = false;
            m_recId = m_records.size();
        } else if (!_lastMoveWasRestore && m_recId > 0) {
            m_recId--;
        }
        _lastMoveWasRestore = false;
        if (m_recId == 0)
            return {};
        terark::fstring fdata = m_records[m_recId-1];
        RecordData data(fdata.data(), fdata.size());
        data.makeOwned();
        return {{RecordId(m_recId), data}};
    }

    boost::optional<Record> seekExact(const RecordId& id) final {
        _lastMoveWasRestore = false;
        _needFirstSeek = false;
        m_recId = id.repr(); // id.repr() == index+1 == m_recId
        if (m_recId == 0)
            return {};
        terark::fstring fdata = m_records[m_recId-1];
        RecordData data(fdata.data(), fdata.size());
        data.makeOwned();
        return {{RecordId(m_recId), data}};
    }

    void save() final {
        if (!_needFirstSeek && !_lastMoveWasRestore)
            _savedId = m_recId == 0 ? size_t(-1) : m_recId;
    }

    void saveUnpositioned() final {
        _savedId = 0;
    }

    bool restore() final {
        if (_savedId == size_t(-1)) {
            m_recId = 0;
            return true;
        }
        m_recId = _savedId;
        _lastMoveWasRestore = m_recId == 0 || m_recId != _savedId;
        return !(_lastMoveWasRestore);
    }

    void detachFromOperationContext() final {}
    void reattachToOperationContext(OperationContext* txn) final {}

private:
    const terark::SortableStrVec& m_records;
    size_t m_recId = 0; // m_recId is in (0, m_records.size()]
    					// 0 means EOF, m_records.size() means begin

    size_t _savedId = size_t(-1);
    bool _needFirstSeek = true;
    bool _lastMoveWasRestore = false;
};

//
// RecordStore
//

RecordData
TerarkWrittingRecordStore::dataFor(OperationContext* txn, const RecordId& loc) const {
	invariant(size_t(loc.repr()) != 0);
	invariant(size_t(loc.repr()) <= m_records.size());
    BSONObj obj = terarkDecodeRecord(m_records[loc.repr() - 1]);
	return RecordData(obj.objdata(), obj.objsize()).getOwned();
}

TerarkWrittingRecordStore::TerarkWrittingRecordStore(StringData ns)
  : RecordStore(ns)
{
	invariant(!NamespaceString::oplog(ns) && "TerarkDB do not support oplog");
}

const char* TerarkWrittingRecordStore::name() const {
    return "TerarkWrittingRecordStore";
}

bool TerarkWrittingRecordStore::findRecord(OperationContext* txn,
										 const RecordId& loc,
										 RecordData* rd) const {
	size_t recId = loc.repr();
    if (recId > m_records.size()) {
        error() << "TerarkWrittingRecordStore::findRecord cannot find record for " << ns() << ":" << loc;
        invariant(!"TerarkWrittingRecordStore::findRecord");
    }
    BSONObj obj = terarkDecodeRecord(m_records[recId-1]);
    *rd = RecordData(obj.objdata(), obj.objsize()).getOwned();
    return true;
}

void TerarkWrittingRecordStore::deleteRecord(OperationContext* txn, const RecordId& loc) {
    error() << "TerarkWrittingRecordStore::deleteRecord is not supported";
    invariant(!"TerarkWrittingRecordStore::deleteRecord is not supported");
}

StatusWith<RecordId> TerarkWrittingRecordStore::insertRecord(OperationContext* txn,
														   const char* data,
														   int len,
														   bool enforceQuota) {
//	log() << "(int&)data[0] = " << (int&)data[0] << ", len = " << len;
	invariant((int&)data[0] == len);

	BSONObj obj(data);
	terark::valvec<char> encoded;
	terarkEncodeRecord(obj, &encoded);

	BSONObj obj2 = terarkDecodeRecord(encoded);
//	log() << "len=" << len << ", encodedLen=" << encoded.size() << ", decodedLen=" << obj2.objsize();
//	log() << toHexLower(obj.objdata(), obj.objsize());
//	log() << toHexLower(obj2.objdata(), obj2.objsize());
//	log() << "obj1: " << tojson(obj);
//	log() << "obj2: " << tojson(obj2);
	invariant(memcmp(obj2.objdata(), obj.objdata(), len) == 0);

	m_records.push_back(encoded);

	size_t recId = m_records.size(); // recId >= 1

//	std::string ns = txn->getNS();
//	log() << BOOST_CURRENT_FUNCTION << ", txn->getNS()=" << ns << ", recId=" << recId << ", datalen=" << len;

    return StatusWith<RecordId>(RecordId(recId));
}

StatusWith<RecordId> TerarkWrittingRecordStore::insertRecord(OperationContext* txn,
														   const DocWriter* doc,
														   bool enforceQuota) {
    const size_t len = doc->documentSize();
    terark::valvec<char> rec(len, terark::valvec_no_init());
    doc->writeDocument(rec.data());

	m_records.push_back(terarkEncodeRecord(BSONObj(rec.data())));

	size_t recId = m_records.size(); // recId >= 1

//	std::string ns = txn->getNS();
//	log() << BOOST_CURRENT_FUNCTION << ", txn->getNS()=" << ns << ", recId=" << recId << ", datalen=" << len;

    return StatusWith<RecordId>(RecordId(recId));
}

StatusWith<RecordId> TerarkWrittingRecordStore::updateRecord(OperationContext* txn,
														   const RecordId& loc,
														   const char* data,
														   int len,
														   bool enforceQuota,
														   UpdateNotifier* notifier) {
	std::string ns = txn->getNS();
	log() << "updateRecord: txn->getNS()=" << ns << ", recId=" << loc.repr() << ", datalen=" << len;
	if (size_t(loc.repr()) < m_records.size()) {
		return StatusWith<RecordId>(ErrorCodes::CommandNotSupported,
				"updateRecord: TerarkStorageEngine is simple binlog", 10003);
	}
	else {
		m_records.pop_back();
		m_records.push_back(terarkEncodeRecord(BSONObj(data)));
	    return StatusWith<RecordId>(loc);
	}
}

bool TerarkWrittingRecordStore::updateWithDamagesSupported() const {
    return false;
}

StatusWith<RecordData> TerarkWrittingRecordStore::updateWithDamages(
						OperationContext* txn,
						const RecordId& loc,
						const RecordData& oldRec,
						const char* damageSource,
						const mutablebson::DamageVector& damages) {
	return Status(ErrorCodes::CommandNotSupported,
			"updateWithDamages: TerarkStorageEngine is simple binlog", 10003);
}

std::unique_ptr<SeekableRecordCursor>
TerarkWrittingRecordStore::getCursor(OperationContext* txn, bool forward) const {
    if (forward)
        return std::unique_ptr<SeekableRecordCursor>(new Cursor(m_records));
    else
    	return std::unique_ptr<SeekableRecordCursor>(new ReverseCursor(m_records));
}

Status TerarkWrittingRecordStore::truncate(OperationContext* txn) {
	return Status(ErrorCodes::CommandNotSupported,
			"truncate: TerarkStorageEngine is simple binlog", 10003);
}

void TerarkWrittingRecordStore::temp_cappedTruncateAfter(OperationContext* txn,
                                                   RecordId end,
                                                   bool inclusive) {
	throw Status(ErrorCodes::CommandNotSupported,
			"temp_cappedTruncateAfter: TerarkStorageEngine is simple binlog", 10003);
}

Status TerarkWrittingRecordStore::validate(OperationContext* txn,
										 bool full,
										 bool scanData,
										 ValidateAdaptor* adaptor,
										 ValidateResults* results,
										 BSONObjBuilder* output) {
    results->valid = true;
    return Status::OK();
}

void TerarkWrittingRecordStore::appendCustomStats(OperationContext* txn,
												BSONObjBuilder* result,
												double scale) const {
}

Status TerarkWrittingRecordStore::touch(OperationContext* txn, BSONObjBuilder* output) const {
    if (output) {
        output->append("numRanges", 1);
        output->append("millis", 0);
    }
    return Status::OK();
}

int64_t TerarkWrittingRecordStore::storageSize(OperationContext* txn,
											 BSONObjBuilder* extraInfo,
											 int infoLevel) const {
	return m_records.mem_size();
}

boost::optional<RecordId>
TerarkWrittingRecordStore::oplogStartHack(OperationContext* txn, const RecordId&) const {
	return {};
}

void TerarkWrittingRecordStore::finishRecordStore(boost::filesystem::path& dbPath, terark::fstring ident) {
	using namespace terark;
//	setenv("NestLoudsTrie_split_by_crlf",
	log() << "TerarkWrittingRecordStore::finishRecordStore: m_records.str_size="
			<< m_records.str_size() << ", recNum=" << m_records.size();
	size_t maxTrie;
	if (const char* env = getenv("TerarkRecordStore_maxTrie")) {
		maxTrie = atoi(env);
		if (maxTrie > 1000) {
			log() << "TerarkRecordStore_maxTrie=" << maxTrie << " is too large, set to 1000";
			maxTrie = 1000;
		}
	} else {
		double avgLen = double(m_records.str_size()) / m_records.size();
		maxTrie = (size_t)ceil(log2(avgLen));
	}
	uint32_t recNum = uint32_t(m_records.size());
	uint64_t dataSize = m_records.str_size();
	valvec<uint32_t> idvec;
	NestLoudsTrie_SE_512 trie;
	NestLoudsTrieConfig conf;
	conf.initFromEnv();
	conf.nestLevel = maxTrie;
	trie.build_strpool(m_records, idvec, conf);
	size_t trieDataBytes = 0;
	AutoFree<byte_t> trieData(trie.save_mmap(&trieDataBytes));
	auto fname = dbPath / (ident + ".data");
	FileStream fp(fname.string().c_str(), "wb");
	invariant(idvec.size() == recNum);
	fp.ensureWrite(&dataSize, 8);
	fp.ensureWrite(&recNum, 4);
	fp.ensureWrite(idvec.data(), idvec.used_mem_size());
	fp.ensureWrite(trieData, trieDataBytes);
	m_records.clear();
}

}  // namespace mongo

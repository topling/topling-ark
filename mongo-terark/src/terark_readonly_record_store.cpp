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

#include "terark_readonly_record_store.h"
#include "record_codec.h"

#include "mongo/db/jsobj.h"
#include "mongo/db/namespace_string.h"
#include "mongo/db/operation_context.h"
#include "mongo/db/storage/oplog_hack.h"
#include "mongo/db/storage/recovery_unit.h"
#include "mongo/stdx/memory.h"
#include "mongo/util/log.h"
#include "mongo/util/mongoutils/str.h"
#include "mongo/util/unowned_ptr.h"
#include <terark/util/mmap.hpp>

namespace mongo {

using std::shared_ptr;
using namespace terark;

class TerarkReadonlyRecordStore::InsertChange : public RecoveryUnit::Change {
public:
    virtual void commit() {
    	invariant(!"TerarkReadonlyRecordStore::InsertChange never commit");
    }
    virtual void rollback() {
    	invariant(!"TerarkReadonlyRecordStore::InsertChange never rollback");
    }
};

// Works for both removes and updates
class TerarkReadonlyRecordStore::RemoveChange : public RecoveryUnit::Change {
public:
    virtual void commit() {
    	invariant(!"TerarkReadonlyRecordStore should never RemoveChange");
    }
    virtual void rollback() {
    	invariant(!"TerarkReadonlyRecordStore should never rollback");
    }
};

class TerarkReadonlyRecordStore::TruncateChange : public RecoveryUnit::Change {
public:
    virtual void commit() {
    	invariant(!"TerarkReadonlyRecordStore should never TruncateChange");
    }
    virtual void rollback() {
    	invariant(!"TerarkReadonlyRecordStore should never rollback");
    }
};

class TerarkReadonlyRecordStore::ForwardCursor final : public SeekableRecordCursor {
public:
    ForwardCursor(const TerarkReadonlyRecordStore& rs)
        : m_rs(rs), _savedId(rs.m_recIdToNodeId.size()) {}

    boost::optional<Record> next() final {
        if (_needFirstSeek) {
            _needFirstSeek = false;
            m_recId = 0;
        } else if (!_lastMoveWasRestore && m_recId != m_rs.m_recordsNum) {
            m_recId++;
        }
        _lastMoveWasRestore = false;

        if (m_recId == m_rs.m_recIdToNodeId.size())
            return {};

        return {Record{RecordId(m_recId+1), m_rs.getRecordData(m_recId)}};
    }

    boost::optional<Record> seekExact(const RecordId& id) final {
        _lastMoveWasRestore = false;
        _needFirstSeek = false;
        m_recId = id.repr() - 1;
        if (m_recId == m_rs.m_recIdToNodeId.size())
            return {};
        return {Record{RecordId(m_recId+1), m_rs.getRecordData(m_recId)}};
    }

    void save() final {
        if (!_needFirstSeek && !_lastMoveWasRestore)
            _savedId = m_recId;
    }

    void saveUnpositioned() final {
        _savedId = m_rs.m_recIdToNodeId.size();
    }

    bool restore() final {
        if (_savedId == m_rs.m_recIdToNodeId.size()) {
            m_recId = m_rs.m_recIdToNodeId.size();
            return true;
        }

        m_recId = _savedId;
        _lastMoveWasRestore = m_recId == m_rs.m_recIdToNodeId.size();

        return !(_lastMoveWasRestore);
    }

    void detachFromOperationContext() final {}
    void reattachToOperationContext(OperationContext* txn) final {}

private:
    const TerarkReadonlyRecordStore& m_rs;
    size_t m_recId = 0;
    size_t _savedId;
    bool _needFirstSeek = true;
    bool _lastMoveWasRestore = false;
};

class TerarkReadonlyRecordStore::ReverseCursor final : public SeekableRecordCursor {
public:
    ReverseCursor(const TerarkReadonlyRecordStore& rs)
        : m_rs(rs), _savedId(0) {}

    boost::optional<Record> next() final {
        if (_needFirstSeek) {
            _needFirstSeek = false;
            m_recId = m_rs.m_recIdToNodeId.size();
        } else if (!_lastMoveWasRestore && m_recId > 0) {
            m_recId--;
        }
        _lastMoveWasRestore = false;

        if (m_recId == 0)
            return {};

        return {Record{RecordId(m_recId), m_rs.getRecordData(m_recId-1)}};
    }

    boost::optional<Record> seekExact(const RecordId& id) final {
        _lastMoveWasRestore = false;
        _needFirstSeek = false;
        m_recId = id.repr();
        if (m_recId == 0)
            return {};
        return {Record{RecordId(m_recId), m_rs.getRecordData(m_recId-1)}};
    }

    void save() final {
        if (!_needFirstSeek && !_lastMoveWasRestore)
            _savedId = m_recId;
    }

    void saveUnpositioned() final {
        _savedId = 0;
    }

    bool restore() final {
        if (_savedId == 0) {
            m_recId = 0;
            return true;
        }

        m_recId = _savedId;
        _lastMoveWasRestore = m_recId == 0;

        return !(_lastMoveWasRestore);
    }

    void detachFromOperationContext() final {}
    void reattachToOperationContext(OperationContext* txn) final {}

private:
    const TerarkReadonlyRecordStore& m_rs;
    size_t m_recId = 0;
    size_t _savedId;
    bool _needFirstSeek = true;
    bool _lastMoveWasRestore = false;
};


TerarkReadonlyRecordStore::TerarkReadonlyRecordStore(StringData ns, const path& dbPath, fstring ident)
  : RecordStore(ns)
{
	auto dbFile = dbPath / (ident + ".data");
	log() << "loading file: " << dbFile.string();
	m_mmapBase = (byte_t*)terark::mmap_load(dbFile.string(), &m_mmapSize);
	m_unzipped_data_size = *(uint64_t*)(m_mmapBase);
	size_t recNum = *(uint32_t*)(m_mmapBase + 8);
	m_recIdToNodeId.risk_set_data((uint32_t*)(m_mmapBase + 8 + 4), recNum);
	size_t len1 = 8 + 4 + m_recIdToNodeId.used_mem_size();
	log() << "recNum=" << recNum << ", len1=" << len1 << ", mmapSize=" << m_mmapSize
			<< ", unzipped_data_size=" << m_unzipped_data_size;
	m_records.load_mmap(m_mmapBase + len1, m_mmapSize - len1);
	m_recordsNum = recNum;
	log() << "TerarkReadonlyRecordStore: done";
}

TerarkReadonlyRecordStore::~TerarkReadonlyRecordStore() {
	m_records.risk_release_ownership();
	m_recIdToNodeId.risk_release_ownership();
	terark::mmap_close(m_mmapBase, m_mmapSize);
	m_mmapBase = NULL;
	m_mmapSize = 0;
}

const char* TerarkReadonlyRecordStore::name() const {
    return "Terark";
}

RecordData
TerarkReadonlyRecordStore::dataFor(OperationContext*, const RecordId& loc) const {
	invariant(size_t(loc.repr()) > 0);
	invariant(size_t(loc.repr()) <= m_recIdToNodeId.size());
	size_t recId = loc.repr() - 1;
	return getRecordData(recId);
}

bool TerarkReadonlyRecordStore::findRecord(OperationContext* txn,
										 const RecordId& loc,
										 RecordData* rd) const {
	if (loc.repr() > 0 && size_t(loc.repr()) <= m_recIdToNodeId.size()) {
		size_t recId = loc.repr() - 1;
	    *rd = getRecordData(recId);
	    return true;
	}
	return false;
}

void TerarkReadonlyRecordStore::deleteRecord(OperationContext*, const RecordId&) {
    invariant(!"TerarkReadonlyRecordStore::deleteRecord is not supported");
}

StatusWith<RecordId>
TerarkReadonlyRecordStore::insertRecord(OperationContext*, const char*, int, bool) {
	log() << BOOST_CURRENT_FUNCTION << ": tracing";
    return StatusWith<RecordId>(ErrorCodes::CommandNotSupported,
    		"TerarkReadonlyRecordStore::insertRecord(data,len)");
}

StatusWith<RecordId>
TerarkReadonlyRecordStore::insertRecord(OperationContext*, const DocWriter*, bool) {
	log() << BOOST_CURRENT_FUNCTION << ": tracing";
    return StatusWith<RecordId>(ErrorCodes::CommandNotSupported,
    		"TerarkReadonlyRecordStore::insertRecord(DocWriter)");
}

StatusWith<RecordId>
TerarkReadonlyRecordStore::updateRecord(OperationContext*, const RecordId&,
                                      const char*, int, bool, UpdateNotifier*) {
	log() << BOOST_CURRENT_FUNCTION << ": tracing";
	return StatusWith<RecordId>(ErrorCodes::CommandNotSupported,
			"updateRecord: TerarkStorageEngine is readonly", 10003);
}

bool TerarkReadonlyRecordStore::updateWithDamagesSupported() const {
    return false;
}

StatusWith<RecordData> TerarkReadonlyRecordStore::updateWithDamages(
							OperationContext* txn,
							const RecordId& loc,
							const RecordData& oldRec,
							const char* damageSource,
							const mutablebson::DamageVector&) {
	log() << BOOST_CURRENT_FUNCTION << ": tracing";
	return Status(ErrorCodes::CommandNotSupported,
			"updateWithDamages: TerarkStorageEngine is readonly", 10003);
}

std::unique_ptr<SeekableRecordCursor>
TerarkReadonlyRecordStore::getCursor(OperationContext*, bool forward) const {
    if (forward)
        return std::unique_ptr<SeekableRecordCursor>(new ForwardCursor(*this));
    else
    	return std::unique_ptr<SeekableRecordCursor>(new ReverseCursor(*this));
}

Status TerarkReadonlyRecordStore::truncate(OperationContext*) {
	log() << BOOST_CURRENT_FUNCTION << ": tracing";
	return Status(ErrorCodes::CommandNotSupported,
			"truncate: TerarkStorageEngine is readonly", 10003);
}

void TerarkReadonlyRecordStore::temp_cappedTruncateAfter(OperationContext* txn,
													   RecordId end,
													   bool inclusive) {
	log() << BOOST_CURRENT_FUNCTION << ": tracing";
	throw Status(ErrorCodes::CommandNotSupported,
			"temp_cappedTruncateAfter: TerarkStorageEngine is readonly", 10003);
}

Status TerarkReadonlyRecordStore::validate(OperationContext* txn,
                                     bool full,
                                     bool scanData,
                                     ValidateAdaptor* adaptor,
                                     ValidateResults* results,
                                     BSONObjBuilder* output) {
	log() << BOOST_CURRENT_FUNCTION << ": tracing";
    return Status(ErrorCodes::CommandNotSupported,
			"truncate: TerarkStorageEngine is readonly", 10003);
}

void TerarkReadonlyRecordStore::appendCustomStats(OperationContext* txn,
                                            	BSONObjBuilder* result,
												double scale) const {
}

Status
TerarkReadonlyRecordStore::touch(OperationContext*, BSONObjBuilder* output) const {
    if (output) {
        output->append("numRanges", 1);
        output->append("millis", 0);
    }
    return Status::OK();
}

void TerarkReadonlyRecordStore::increaseStorageSize(OperationContext*, int, bool) {
    invariant(!"increaseStorageSize not yet implemented");
}

int64_t TerarkReadonlyRecordStore::storageSize(OperationContext* txn,
                                         	 BSONObjBuilder* extraInfo,
											 int infoLevel) const {
	if (extraInfo) {
		extraInfo->append(StringData("node.num"), (long long)m_records.total_states());
	}
	return m_records.mem_size() + m_recIdToNodeId.used_mem_size();
}

boost::optional<RecordId>
TerarkReadonlyRecordStore::oplogStartHack(OperationContext* txn, const RecordId&) const {
	return {};
}

void TerarkReadonlyRecordStore::getRawData(size_t recId, terark::valvec<char>* data) const {
    size_t nodeId = m_recIdToNodeId[recId];
    m_records.restore_string(nodeId, (terark::valvec<unsigned char>*)data);
}

RecordData TerarkReadonlyRecordStore::getRecordData(size_t recId) const {
    terark::valvec<char> fdata;
    getRawData(recId, &fdata);
    BSONObj obj = terarkDecodeRecord(fdata.data(), fdata.size());
    return RecordData(obj.objdata(), obj.objsize()).getOwned();
}


}  // namespace mongo

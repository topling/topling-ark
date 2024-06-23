// terark_record_store.h

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

#pragma once

#include <boost/shared_array.hpp>
#include <boost/filesystem/path.hpp>
#include <map>

#include "mongo/db/storage/capped_callback.h"
#include "mongo/db/storage/record_store.h"

#include <terark/fsa/nest_louds_trie.hpp>

namespace mongo {

/**
 * A RecordStore that stores all data in-memory.
 *
 * @param cappedMaxSize - required if isCapped. limit uses dataSize() in this impl.
 */
class TerarkWrittingRecordStore : public RecordStore {
public:
    explicit TerarkWrittingRecordStore(StringData ns);

    const char* name() const override;

    RecordData dataFor(OperationContext* txn, const RecordId& loc) const override;

    bool findRecord(OperationContext* txn, const RecordId& loc, RecordData* rd) const override;

    void deleteRecord(OperationContext* txn, const RecordId& dl) override;

    StatusWith<RecordId> insertRecord(OperationContext* txn,
                                      const char* data,
                                      int len,
                                      bool enforceQuota) override;

    StatusWith<RecordId> insertRecord(OperationContext* txn,
                                      const DocWriter* doc,
                                      bool enforceQuota) override;

    StatusWith<RecordId> updateRecord(OperationContext* txn,
                                      const RecordId& oldLocation,
                                      const char* data,
                                      int len,
                                      bool enforceQuota,
                                      UpdateNotifier* notifier) override;

    bool updateWithDamagesSupported() const override;

    StatusWith<RecordData> updateWithDamages(OperationContext* txn,
                             const RecordId& loc,
                             const RecordData& oldRec,
                             const char* damageSource,
                             const mutablebson::DamageVector& damages) override;

    std::unique_ptr<SeekableRecordCursor>
    getCursor(OperationContext* txn, bool forward) const override;

    Status truncate(OperationContext* txn) override;

    void temp_cappedTruncateAfter(OperationContext*, RecordId end, bool) override;

    Status validate(OperationContext*, bool full, bool scanData,
                    ValidateAdaptor*, ValidateResults*, BSONObjBuilder*) override;

    void appendCustomStats(OperationContext*, BSONObjBuilder*, double)const override;

    Status touch(OperationContext*, BSONObjBuilder* output) const override;

    int64_t storageSize(OperationContext*, BSONObjBuilder*, int) const override;
    long long dataSize(OperationContext*) const override { return m_records.str_size(); }
    long long numRecords(OperationContext*) const override { return m_records.size(); }

    boost::optional<RecordId>
    oplogStartHack(OperationContext* txn, const RecordId&) const override;

    void updateStatsAfterRepair(OperationContext*, long long, long long) override {
        invariant(!"TerarkWrittingRecordStore::updateStatsAfterRepair is not supported");
    }

    bool isCapped() const override { return false; }

    void finishRecordStore(boost::filesystem::path& dbPath, terark::fstring ident);

private:
    RecordData getRecordData(size_t recId) const;

    class InsertChange;
    class RemoveChange;
    class TruncateChange;

    class Cursor;
    class ReverseCursor;
    terark::SortableStrVec m_records;
};

}  // namespace mongo

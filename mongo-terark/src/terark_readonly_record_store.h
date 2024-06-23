// terark_record_store.h

/**
*    Copyright (C) 2014 QiJian Inc.
*/

#pragma once

#include <boost/shared_array.hpp>
#include <map>

#include "mongo/db/storage/capped_callback.h"
#include "mongo/db/storage/record_store.h"

#include <terark/fsa/nest_louds_trie.hpp>
#include <boost/filesystem.hpp>

using boost::filesystem::path;

namespace mongo {

class TerarkReadonlyRecordStore : public RecordStore {
public:
    explicit TerarkReadonlyRecordStore(StringData ns, const path& dbPath, terark::fstring ident);
    ~TerarkReadonlyRecordStore();

    const char* name() const override;

    RecordData dataFor(OperationContext*, const RecordId&) const override;
    bool findRecord(OperationContext*, const RecordId&, RecordData*) const override;
    void deleteRecord(OperationContext*, const RecordId&) override;

    StatusWith<RecordId> insertRecord(OperationContext*, const char*, int, bool) override;
    StatusWith<RecordId> insertRecord(OperationContext*, const DocWriter*, bool) override;

    StatusWith<RecordId> updateRecord(OperationContext*,
									  const RecordId& oldLocation,
									  const char* data,
									  int len,
									  bool enforceQuota,
									  UpdateNotifier* notifier) override;

    bool updateWithDamagesSupported() const;

    StatusWith<RecordData> updateWithDamages(OperationContext* txn,
							 const RecordId& loc,
							 const RecordData& oldRec,
							 const char* damageSource,
							 const mutablebson::DamageVector&) override;

    std::unique_ptr<SeekableRecordCursor>
    getCursor(OperationContext* txn, bool forward) const override;

    Status truncate(OperationContext* txn) override;

    void temp_cappedTruncateAfter(OperationContext*, RecordId end, bool) override;

    Status validate(OperationContext*, bool full, bool scanData,
					ValidateAdaptor*, ValidateResults*, BSONObjBuilder*) override;

    void appendCustomStats(OperationContext*, BSONObjBuilder*, double) const override;

    Status touch(OperationContext* txn, BSONObjBuilder* output) const;

    void increaseStorageSize(OperationContext* txn, int size, bool enforceQuota);

    int64_t storageSize(OperationContext*, BSONObjBuilder*, int) const override;
    long long dataSize(OperationContext*) const override { return m_records.mem_size(); }
    long long numRecords(OperationContext*) const override { return m_recIdToNodeId.size(); }

    boost::optional<RecordId>
    oplogStartHack(OperationContext* txn, const RecordId&) const override;

    void updateStatsAfterRepair(OperationContext*, long long, long long) override {
        invariant(!"TerarkReadonlyRecordStore::updateStatsAfterRepair is not supported");
    }

    bool isCapped() const override { return false; }

    // non overrides
    void getRawData(size_t recId, terark::valvec<char>* data) const;

private:
    RecordData getRecordData(size_t recId) const;

    class InsertChange;
    class RemoveChange;
    class TruncateChange;

    class ForwardCursor; friend class ForwardCursor;
    class ReverseCursor; friend class ReverseCursor;

    uint64_t m_unzipped_data_size;
    size_t m_recordsNum;
    terark::NestLoudsTrie_SE_512 m_records;
    terark::valvec<uint32_t> m_recIdToNodeId;
    terark::byte_t* m_mmapBase;
    size_t m_mmapSize;
};

}  // namespace mongo

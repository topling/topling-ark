// terark_engine.h

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

#include "mongo/db/storage/kv/kv_engine.h"
#include "mongo/stdx/mutex.h"
#include "mongo/util/string_map.h"
#include <boost/filesystem/path.hpp>
#include <terark/hash_strmap.hpp>
#include <terark/fsa/nest_louds_trie.hpp>
#include <terark/fsa/nest_trie_dawg.hpp>

namespace mongo {

class TerarkEngine : public KVEngine {
public:
	TerarkEngine(const std::string& dbpath);
	~TerarkEngine();
    RecoveryUnit* newRecoveryUnit() override;

    Status createRecordStore(OperationContext* opCtx,
							 StringData ns,
							 StringData ident,
							 const CollectionOptions&) override;

    RecordStore* getRecordStore(OperationContext* opCtx,
								StringData ns,
								StringData ident,
								const CollectionOptions& options) override;

    Status createSortedDataInterface(OperationContext* opCtx,
									 StringData ident,
									 const IndexDescriptor*) override;

    SortedDataInterface* getSortedDataInterface(OperationContext* opCtx,
												StringData ident,
												const IndexDescriptor*) override;

    Status dropIdent(OperationContext* opCtx, StringData ident) override;

    bool supportsDocLocking() const override { return true; }
    bool supportsDirectoryPerDB() const override { return false; }

    bool isDurable() const override { return true; }

    int64_t getIdentSize(OperationContext*, StringData ident) override;
    Status repairIdent(OperationContext*, StringData)override{return Status::OK();}

    void cleanShutdown() override;

    bool hasIdent(OperationContext* opCtx, StringData ident) const override {
        return !m_metaMap.is_null(ident);
    }

    std::vector<std::string> getAllIdents(OperationContext* opCtx) const override;

private:
    struct MetaEntry {
    	RecordStore* dbData;
    	SortedDataInterface* dbIndex;
    	size_t      storageSize;
    	std::string ns; // namespace
    	BSONObj info;
    	MetaEntry();
    };
    mutable stdx::mutex _mutex;
    boost::filesystem::path m_dbPath;
    terark::hash_strmap_p<MetaEntry> m_metaMap;
    bool m_isBuilding;

    void buildMetaData();
};

}

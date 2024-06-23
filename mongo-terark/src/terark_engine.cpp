// terark_engine.cpp

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

#include "terark_engine.h"

#include <mongo/db/index/index_descriptor.h>
#include <mongo/db/json.h>
#include <mongo/util/log.h>
#include <boost/filesystem.hpp>
#include "terark_btree_impl.h"
#include "terark_readonly_record_store.h"
#include "terark_writting_record_store.h"
#include "terark_recovery_unit.h"
#include "record_codec.h"
#include <typeinfo>

namespace mongo {

namespace fs = boost::filesystem;

TerarkEngine::MetaEntry::MetaEntry() {
	dbData = NULL;
	dbIndex = NULL;
	storageSize = 0;
}

bool isBrainDeadNamespace(StringData ns) {
	return ns.startsWith("local.") || ns == "_mdb_catalog";
}

TerarkEngine::TerarkEngine(const std::string& dbPath) : m_dbPath(dbPath) {
	using namespace terark;
    stdx::lock_guard<stdx::mutex> lk(_mutex);
	fs::path metaFile = m_dbPath/"metadata.dfa";
	if (fs::is_regular_file(metaFile)) {
	    std::unique_ptr<AcyclicPathDFA> metaDFA(static_cast<AcyclicPathDFA*>(BaseDFA::load_from(metaFile.string())));
	    valvec<fstring> fields;
	    metaDFA->for_each_word([&](size_t nth, fstring kvData){
	    	kvData.split('\t', &fields);
	    	if (fields.size() != 4) {
	    		THROW_STD(invalid_argument, "Invalid metafile: %s, metadata: %.*s"
	    				, metaFile.c_str(), kvData.ilen(), kvData.data());
	    	}
	    	fstring ident = fields[0];
	    	fstring type  = fields[1]; // data or index
	    	StringData ns(fields[2].p, fields[2].n);
	    	fstring info = fields[3];
	    	if (!m_metaMap.is_null(ident)) {
	    		THROW_STD(invalid_argument, "duplicated ident=%.*s", ident.ilen(), ident.data());
	    	}
	    	std::unique_ptr<MetaEntry> meta(new MetaEntry());
			log() << "metadata line:" << nth << ": " << kvData.str();
			if (type == "index") {
				try {
					std::unique_ptr<SortedDataInterface> readIndex(getTerarkReadOnlyIndex(m_dbPath, ident));
					if (isBrainDeadNamespace(ns)) {
						std::unique_ptr<SortedDataInterface> writeIndex(getTerarkWriteOnlyIndex(m_dbPath, ident));
						log() << "mongodb brain dead index:" << ident.str() << " namespace: " << ns;
						if (ns.startsWith("local.")) {
						//	return; // do not load local index
						}
						meta->dbIndex = writeIndex.release();
					} else {
						meta->storageSize = readIndex->getSpaceUsedBytes(NULL);
						meta->dbIndex = readIndex.release();
					}
				} catch (const std::exception& ex) {
					fprintf(stderr
						, "failed load %.*s, namespace=%.*s, caught exception: %s\n"
						, ident.ilen(), ident.data(), (int)ns.size(), ns.rawData()
						, ex.what());
					meta.reset(NULL);
				}
			} else {
				std::unique_ptr<TerarkReadonlyRecordStore> readStore(new TerarkReadonlyRecordStore(ns, dbPath, ident));
				if (isBrainDeadNamespace(ns)) {
					std::unique_ptr<TerarkWrittingRecordStore> writeStore(new TerarkWrittingRecordStore(ns));
					valvec<char> data;
					for (long long id = 0; id < readStore->numRecords(NULL); ++id) {
						readStore->getRawData(id, &data);
						BSONObj bobj(terarkDecodeRecord(data));
						log() << "namespace: " << ns << ": recId=" << id << ", json: " << tojson(bobj);
						writeStore->insertRecord(NULL, bobj.objdata(), bobj.objsize(), true);
					}
					if (ns.startsWith("local.")) {
					//	return; // do not load local index
					}
					meta->dbData = writeStore.release();
				} else {
					meta->storageSize = readStore->storageSize(NULL, NULL, 1);
					meta->dbData = readStore.release();
				}
			}
			if (meta) {
				meta->ns = ns.toString();
				meta->info = fromjson(info.p);
				m_metaMap.replace(ident, meta.release());
			}
			log() << "metadata line:" << nth << ": " << kvData.str() << " Done!";
	    });
	    m_isBuilding = false;
	} else {
	    m_isBuilding = true;
	}
}

TerarkEngine::~TerarkEngine() {
}

RecoveryUnit* TerarkEngine::newRecoveryUnit() {
    return new TerarkRecoveryUnit();
}

Status TerarkEngine::createRecordStore(OperationContext* opCtx,
                                     StringData ns,
                                     StringData ident,
                                     const CollectionOptions& options) {
    stdx::lock_guard<stdx::mutex> lk(_mutex);
    if (!m_isBuilding && !isBrainDeadNamespace(ns)) {
    	log() << "TerarkEngine::createRecordStore: TerarkEngine is readonly";
    	return Status(ErrorCodes::CommandNotSupported, "TerarkEngine is readonly");
    }
    auto ib = m_metaMap.get_map().insert_i(ident);
    MetaEntry*& meta = m_metaMap.get_map().val(ib.first);
    if (ib.second) {
    	meta = new MetaEntry();
    	meta->dbData = new TerarkWrittingRecordStore(ns);
    	meta->ns = ns.toString();
    }
    else {
    	invariant(StringData(meta->ns) == ns);
    }
    invariant(!meta->dbIndex);
    return Status::OK();
}

RecordStore* TerarkEngine::getRecordStore(OperationContext* opCtx,
                                        StringData ns,
                                        StringData ident,
                                        const CollectionOptions& options) {
    stdx::lock_guard<stdx::mutex> lk(_mutex);
    MetaEntry* meta = m_metaMap[ident];
    return meta ? meta->dbData : NULL;
}

Status TerarkEngine::createSortedDataInterface(OperationContext* opCtx,
                                             StringData ident,
                                             const IndexDescriptor* desc) {
    stdx::lock_guard<stdx::mutex> lk(_mutex);
    if (!m_isBuilding && !isBrainDeadNamespace(desc->parentNS())) {
    	log() << "TerarkEngine::createSortedDataInterface: TerarkEngine is readonly";
    	return Status(ErrorCodes::CommandNotSupported, "TerarkEngine is readonly");
    }
    auto ib = m_metaMap.get_map().insert_i(ident);
    MetaEntry*& meta = m_metaMap.get_map().val(ib.first);
    if (ib.second) {
    	meta = new MetaEntry();
    	meta->dbIndex = getTerarkWriteOnlyIndex(m_dbPath, ident);
    	meta->info = desc->infoObj().copy();
    	meta->ns = desc->parentNS();
    }
    else {
    	invariant(desc->parentNS() == meta->ns);
    }
    invariant(!meta->dbData);
    return Status::OK();
}

SortedDataInterface*
TerarkEngine::getSortedDataInterface(OperationContext* opCtx,
								   StringData ident,
                                   const IndexDescriptor* desc) {
    stdx::lock_guard<stdx::mutex> lk(_mutex);
    MetaEntry* meta = m_metaMap[ident];
    if (meta) {
		invariant(desc->parentNS() == meta->ns);
		return meta->dbIndex;
    }
    return NULL;
}

Status TerarkEngine::dropIdent(OperationContext* opCtx, StringData ident) {
	if (m_isBuilding)
		return Status(ErrorCodes::CommandNotSupported,
				"TerarkEngine::dropIdent: TerarkEngine does not support dropIndex when building");
	else
		return Status(ErrorCodes::CommandNotSupported,
				"TerarkEngine::dropIdent: TerarkEngine is readonly");
}

int64_t TerarkEngine::getIdentSize(OperationContext* opCtx, StringData ident) {
	invariant(!m_metaMap.is_null(ident));
	const MetaEntry* meta = m_metaMap[ident];
    return meta->storageSize;
}

using namespace terark;
using namespace std;

vector<string> TerarkEngine::getAllIdents(OperationContext* opCtx) const {
    vector<string> all; all.reserve(m_metaMap.size());
	stdx::lock_guard<stdx::mutex> lk(_mutex);
	for (size_t i = m_metaMap.get_map().beg_i(); m_metaMap.get_map().end_i() != i; ++i) {
		all.push_back(m_metaMap.get_map().key(i).str());
	}
    return all;
}

// 创建一个 meta data store
// 多个 payload data store, 这样可以创建更大的单个库
// 因为如果以 prefix 来作为 meta data 的话，所有 index 和 payload data 都在同一个 DFA 中
// 而单个 DFA 的 max_state 是以 MAX_UINT32-1, 单个库太大时会突破这个限制

void TerarkEngine::buildMetaData() {
	terark::NestLoudsTrieDAWG_SE_512 trie;
	terark::SortableStrVec strVec;
	for (size_t i = m_metaMap.get_map().beg_i(); m_metaMap.get_map().end_i() != i; ++i) {
		fstring ident = m_metaMap.get_map().key(i);
		MetaEntry& meta = *m_metaMap.get_map().val(i);
		invariant(meta.dbData || meta.dbIndex);
		strVec.push_back(ident);
		if (m_metaMap.get_map().val(i)->dbData) {
			strVec.back_append("\tdata\t");
		}
		else {
			strVec.back_append("\tindex\t");
		}
		strVec.back_append(meta.ns);
		strVec.back_append("\t");
		string info = tojson(meta.info);
		strVec.back_append(info);
	}
	terark::NestLoudsTrieConfig conf;
	trie.build_from(strVec, conf);
	trie.save_mmap((m_dbPath / "metadata.dfa").string().c_str());
}

void TerarkEngine::cleanShutdown() {
	if (!m_isBuilding) {
		return;
	}
	buildMetaData();
	for (size_t i = m_metaMap.get_map().beg_i(); m_metaMap.get_map().end_i() != i; ++i) {
		fstring ident = m_metaMap.get_map().key(i);
		MetaEntry& meta = *m_metaMap.get_map().val(i);
		invariant(meta.dbData || meta.dbIndex);
		string info = tojson(meta.info);
		log() << BOOST_CURRENT_FUNCTION << ": flush record store: i=" << i
			  << ", ident=" << ident.c_str()
			  << ", type=" << (meta.dbData ? "data" : "index")
			  << ", info=" << info
			  ;
	}
	log() << BOOST_CURRENT_FUNCTION << ": Building RecordStore ...";
	for (size_t i = m_metaMap.get_map().beg_i(); m_metaMap.get_map().end_i() != i; ++i) {
		fstring ident = m_metaMap.get_map().key(i);
		MetaEntry& meta = *m_metaMap.get_map().val(i);
		invariant(meta.dbData || meta.dbIndex);
		if (meta.dbData) {
		//	auto& db = *meta.dbData;
			log() << BOOST_CURRENT_FUNCTION << ": flush record store: i=" << i << ", ident=" << ident.c_str();
		//	log() << BOOST_CURRENT_FUNCTION << ": storage ns=" << db.ns();
		//	log() << BOOST_CURRENT_FUNCTION << ": storageClass: " << typeid(db).name();
			auto rs = dynamic_cast<TerarkWrittingRecordStore*>(meta.dbData);
		//	invariant(NULL != rs);
			if (rs)
				rs->finishRecordStore(m_dbPath, ident);
			log() << BOOST_CURRENT_FUNCTION << ": record store end: i=" << i;
		}
	}
	log() << BOOST_CURRENT_FUNCTION << ": Building Index ...";
	for (size_t i = m_metaMap.get_map().beg_i(); m_metaMap.get_map().end_i() != i; ++i) {
		fstring ident = m_metaMap.get_map().key(i);
		MetaEntry& meta = *m_metaMap.get_map().val(i);
		invariant(meta.dbData || meta.dbIndex);
		if (meta.dbIndex) {
			finishWriteOnlyIndex(m_dbPath, ident, meta.dbIndex);
		}
	}
//	buildMetaData();
}

}

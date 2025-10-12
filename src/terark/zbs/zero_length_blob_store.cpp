/*
 * plain_blob_store.cpp
 *
 *  Created on: 2017-02-10
 *      Author: leipeng
 */

#include "zero_length_blob_store.hpp"
#include "blob_store_file_header.hpp"
#include <terark/io/FileStream.hpp>
#include <terark/util/checksum_exception.hpp>
#include <terark/util/mmap.hpp>
#include <terark/zbs/xxhash_helper.hpp>
#include <terark/io/StreamBuffer.hpp>

namespace terark {

REGISTER_BlobStore(ZeroLengthBlobStore);

void ZeroLengthBlobStore::init_from_memory(fstring dataMem, Dictionary/*dict*/) {
    m_mmapBase = (FileHeaderBase*)dataMem.p;
    m_numRecords = m_mmapBase->records;
}

void ZeroLengthBlobStore::get_meta_blocks(valvec<Block>* blocks) const {
    blocks->erase_all();
}

void ZeroLengthBlobStore::get_data_blocks(valvec<Block>* blocks) const {
    blocks->erase_all();
}

void ZeroLengthBlobStore::detach_meta_blocks(const valvec<Block>& blocks) {
    assert(blocks.empty());
}


void ZeroLengthBlobStore::save_mmap(function<void(const void*, size_t)> write) const {
    FileHeaderBase header;
    memset(&header, 0, sizeof header);
    header.magic_len = MagicStrLen;
    strcpy(header.magic, MagicString);
    strcpy(header.className, "ZeroLengthBlobStore");
    header.fileSize = sizeof(FileHeaderBase);
    header.records = m_numRecords;

    write(&header, sizeof header);
}

ZeroLengthBlobStore::ZeroLengthBlobStore() {
    m_supportZeroCopy = true;
    m_get_record_append = BlobStoreStaticCastPMF(get_record_append_func_t,
                &ZeroLengthBlobStore::get_record_append_imp);
    m_get_record_append_fiber_vm_prefetch = BlobStoreStaticCastPMF(
                get_record_append_func_t,
                &ZeroLengthBlobStore::get_record_append_imp);
    // binary compatible:
    m_get_record_append_CacheOffsets =
        reinterpret_cast<get_record_append_CacheOffsets_func_t>
        (m_get_record_append);
    m_fspread_record_append = BlobStoreStaticCastPMF(
               fspread_record_append_func_t,
               &ZeroLengthBlobStore::fspread_record_append_imp);
    m_get_zipped_size = BlobStoreStaticCastPMF(get_zipped_size_func_t,
               &ZeroLengthBlobStore::get_zipped_size_imp);
}

ZeroLengthBlobStore::~ZeroLengthBlobStore() {
    if (m_isUserMem) {
        if (m_isMmapData) {
            mmap_close((void*)m_mmapBase, m_mmapBase->fileSize);
        }
        m_mmapBase = nullptr;
        m_isMmapData = false;
        m_isUserMem = false;
    }
}

void ZeroLengthBlobStore::finish(size_t records) {
    m_numRecords = records;
}

size_t ZeroLengthBlobStore::mem_size() const {
    return 0;
}

void
ZeroLengthBlobStore::get_record_append_imp(size_t recID, valvec<byte_t>* recData)
const {
    assert(recID < m_numRecords);
    TERARK_ASSERT_EQ(0, recData->size());
    TERARK_ASSERT_EQ(0, recData->capacity());
}

void
ZeroLengthBlobStore::fspread_record_append_imp(
    pread_func_t fspread, void* lambda,
    size_t baseOffset, size_t recID,
    valvec<byte_t>* recData,
    valvec<byte_t>* rdbuf)
const {
    assert(recID < m_numRecords);
}

size_t
ZeroLengthBlobStore::get_zipped_size_imp(size_t recID, CacheOffsets*) const {
    return 0;
}

void ZeroLengthBlobStore::reorder_zip_data(ZReorderMap& newToOld,
        function<void(const void* data, size_t size)> writeAppend,
        fstring tmpFile)
const {
    save_mmap(writeAppend);
}

} // namespace terark

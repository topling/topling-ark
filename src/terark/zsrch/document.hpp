#pragma once

#include <terark/io/DataIO.hpp>
#include <terark/hash_strmap.hpp>
#include <terark/util/fstrvec.hpp>

namespace terark { namespace zsrch {

class CompactDocFields {
public:
	struct OffsetNode {
		unsigned offset;
		unsigned uintVal;
	};
	struct OffsetNode_StrVecOP {
		size_t get(const OffsetNode& x) const { return x.offset; }
		void   set(OffsetNode& x, size_t y) const { x.offset = unsigned(y); }
		void   inc(OffsetNode& x, ptrdiff_t d = 1) const { x.offset += d; }
		OffsetNode make(size_t y) const { return OffsetNode{unsigned(y), 0}; }
		static const unsigned maxpool = unsigned(-1);
	};
	typedef basic_fstrvec<char, OffsetNode, OffsetNode_StrVecOP>
			fstrvec_with_uintVal;

	size_t numFields() const { return m_fields.size(); }
	size_t termListBeg(size_t iField) const { return m_fields.offsets[iField+0].uintVal; }
	size_t termListEnd(size_t iField) const { return m_fields.offsets[iField+1].uintVal; }
	unsigned termFreq(size_t i) const { return m_terms.offsets[i].uintVal; }
	fstring  term(size_t i) const { assert(i < m_terms.size()); return m_terms[i]; }
	fstring  field(size_t i) const { assert(i < m_fields.size()); return m_fields[i]; }
	bool checkCRC() const;
	void fromHash(const hash_strmap<hash_strmap<uint32_t> >& map);

protected:
	fstrvec_with_uintVal m_fields;
	fstrvec_with_uintVal m_terms;
	uint32_t  m_crc32_fields;
	uint32_t  m_crc32_terms;
	void computeCRC(uint32_t* crc32_fields, uint32_t* crc32_terms) const;
};

DATA_IO_DUMP_RAW_MEM_E(CompactDocFields::OffsetNode);
DATA_IO_LOAD_SAVE_E(CompactDocFields, &m_fields &m_terms &m_crc32_fields &m_crc32_terms);

}} // namespace terark::zsrch

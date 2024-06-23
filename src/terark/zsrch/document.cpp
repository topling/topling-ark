#include "document.hpp"
#include <terark/util/crc.hpp>

namespace terark {
	template<class T>
	uint32_t crc32_update(uint32_t init, const T* a, size_t n) {
		return Crc32c_update(init, a, sizeof(T)*n);
	}
}

namespace terark { namespace zsrch {

void CompactDocFields::computeCRC(uint32_t* crc32_fields, uint32_t* crc32_terms) const {
	uint32_t c = 0;
	c = terark::crc32_update(c, m_fields.offsets.data(), m_fields.offsets.size());
	c = terark::crc32_update(c, m_fields.strpool.data(), m_fields.strpool.size());
	*crc32_fields = uint32_t(c);
	c = terark::crc32_update(c, m_terms.offsets.data(), m_terms.offsets.size());
	c = terark::crc32_update(c, m_terms.strpool.data(), m_terms.strpool.size());
	*crc32_terms = uint32_t(c);
}
bool CompactDocFields::checkCRC() const {
	uint32_t crc32_fields, crc32_terms;
	computeCRC(&crc32_fields, &crc32_terms);
	return crc32_fields == m_crc32_fields
		&& crc32_terms  == m_crc32_terms;
}
void CompactDocFields::fromHash(const hash_strmap<hash_strmap<uint32_t> >& map) {
	m_fields.erase_all();
	m_terms.erase_all();
	for(size_t i = 0; i < map.end_i(); ++i) {
		if (map.val(i).empty())
		   	continue;
		m_fields.offsets.back().uintVal = m_terms.size();
		const fstring fieldName = map.key(i);
		const auto&  termToFreq = map.val(i);
		for(size_t j = 0; j < termToFreq.end_i(); ++j) {
			m_terms.offsets.back().uintVal = termToFreq.val(j);
			m_terms.push_back(termToFreq.key(j));
		}
		m_fields.push_back(fieldName);
	}
	m_fields.offsets.back().uintVal = m_terms.size();
	computeCRC(&m_crc32_fields, &m_crc32_terms);
}

} } // namespace terark::zsrch

#ifndef __terark_rockeet_index_Hit_h__
#define __terark_rockeet_index_Hit_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
# pragma warning(disable: 4127)
#endif

#include <terark/util/refcount.hpp>
#include <terark/io/MemStream.hpp>
#include <terark/io/DataIO.hpp>

namespace terark { namespace rockeet {

class TERARK_DLL_EXPORT Hit
{
public:
    virtual ~Hit();

    /**
     * default only docID
     */
    virtual void loadvalue(PortableDataInput<SeekableMemIO>& input);

    uint64_t docID;
};

class TERARK_DLL_EXPORT HitWithFreq : public Hit
{
public:
    uint32_t freq;

    virtual void loadvalue(PortableDataInput<SeekableMemIO>& input);
};

} } // namespace terark::rockeet

#endif // __terark_rockeet_index_Hit_h__

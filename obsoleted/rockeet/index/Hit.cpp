#include "Hit.hpp"

namespace terark { namespace rockeet {

Hit::~Hit()
{

}

void Hit::loadvalue(PortableDataInput<SeekableMemIO>& input)
{

}

void HitWithFreq::loadvalue(PortableDataInput<SeekableMemIO>& input)
{
    var_size_t vf;
    input >> vf;
    freq = vf.t;
}

} } // namespace terark::rockeet

#ifndef __terark_rpc_connection_h__
#define __terark_rpc_connection_h__

#include "../io/IStream.hpp"

namespace terark { namespace rpc {

class Connection
{
	IInputStream*  is;
	IOutputStream* os;

public:

};


} } // namespace terark::rpc

#endif // __terark_rpc_connection_h__



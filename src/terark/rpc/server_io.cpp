/* vim: set tabstop=4 : */
#include "rpc_basic.hpp"
#include <terark/num_to_str.hpp>

namespace terark { namespace rpc {

void TERARK_DLL_EXPORT
incompitible_class_cast(remote_object* y, const char* szRequestClassName)
{
	string_appender<> oss;
	oss << "object[id=" << y->getID() << ", type=" << y->getClassName()
		<< "] is not " << szRequestClassName;
	throw rpc_exception(oss.str());
}


} } // namespace::terark::rpc

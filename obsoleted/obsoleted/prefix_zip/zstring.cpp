/* vim: set tabstop=4 : */
#include "zstring.hpp"
#include <assert.h>

namespace terark { namespace prefix_zip {

#define NULL_TEMPLATE
//////////////////////////////////////////////////////////////////////////

int ZString_EscapeByte(byte diff)
{
	switch (diff)
	{
	default:
		return -1;

	// control char
	case  0: case  1: case  2: case  3: case  4: case  5: case  6: case  7: case  8: case  9:
	case 10: case 11: case 12: case 13: case 14: case 15: case 16: case 17: case 18: case 19:
	case 20: case 21: case 22: case 23:	case 24: case 25: case 26: case 27: case 28: case 29:
	case 30: case 31: case 32:
		return diff;

	// special char
//	case '"':  return 33;
//	case '\'': return 34;
//	case '?':  return 35;
//	case '':   return 36;
	}
}

} } // namespace terark::prefix_zip

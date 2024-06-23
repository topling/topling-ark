#include "adt_meta.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef _MSC_VER
/* Always compile this module for speed, not size */
#pragma optimize("t", on)
#endif

TERARK_DLL_EXPORT
int
terark_adt_compare_less_tag(const struct container_vtab* vtab,
							const struct container_type* cont,
							const void* x,
							const void* y)
{
	fprintf(stderr, "unexpected call: terark_adt_compare_less_tag\n");
	abort();
	return 0; // avoid warning
}



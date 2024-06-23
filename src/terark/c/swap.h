
#define Value_swap CAT_TOKEN2(Value_swap_, VALUE_SIZE)
#define Value_swap_2(x,y) do { register short t; \
    t = *(short*)(x); *(short*)(x) = *(short*)(y); *(short*)(y) = t;\
  } while(0)

#define Value_swap_1(x,y) do { register unsigned char t; \
    t = *(unsigned char*)(x); *(unsigned char*)(x) = *(unsigned char*)(y); *(unsigned char*)(y) = t;\
  } while(0)

#ifdef _MSC_VER
typedef __int64 BK8;
#else
//typedef struct BK8 { unsigned char a[8]; } BK8; // 8 bytes block\n
typedef long long BK8;
#endif


#if defined(__amd64__) || defined(__amd64) || \
	defined(__x86_64__) || defined(__x86_64) || \
	defined(_M_X64) || \
	defined(__ia64__) || defined(_IA64) || defined(__IA64__) || \
	defined(__ia64) ||\
	defined(_M_IA64)

#ifndef TERARK_C_STRUCT_ALIGN
#define TERARK_C_STRUCT_ALIGN 8
#endif

#include "swap64.h"

#else

#ifndef TERARK_C_STRUCT_ALIGN
#define TERARK_C_STRUCT_ALIGN 4
#endif

#include "swap32.h"

#endif // 64bit



//#include "swap64.h"

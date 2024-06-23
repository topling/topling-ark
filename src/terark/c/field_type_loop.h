// input macro:
//	 FIELD_TYPE_FILE

#ifndef VALUE_SIZE
# error must define VALUE_SIZE, if you need not VALUE_SIZE, define it as TERARK_C_MAX_VALUE_SIZE
#endif

#  define FIELD_TYPE_TYPE signed char
#  define FIELD_TYPE_NAME tev_char
#  include FIELD_TYPE_FILE

#  define FIELD_TYPE_TYPE unsigned char
#  define FIELD_TYPE_NAME tev_byte
#  include FIELD_TYPE_FILE

#if VALUE_SIZE >= 2
#  define FIELD_TYPE_TYPE short
#  define FIELD_TYPE_NAME tev_int16
#  include FIELD_TYPE_FILE

#  define FIELD_TYPE_TYPE unsigned short
#  define FIELD_TYPE_NAME tev_uint16
#  include FIELD_TYPE_FILE
#endif

#if VALUE_SIZE >= 4
#  define FIELD_TYPE_TYPE int
#  define FIELD_TYPE_NAME tev_int32
#  include FIELD_TYPE_FILE

#  define FIELD_TYPE_TYPE unsigned int
#  define FIELD_TYPE_NAME tev_uint32
#  include FIELD_TYPE_FILE
#endif

#if VALUE_SIZE >= 8
#  define FIELD_TYPE_TYPE terark_int64_t
#  define FIELD_TYPE_NAME tev_int64
#  include FIELD_TYPE_FILE

#  define FIELD_TYPE_TYPE terark_uint64_t
#  define FIELD_TYPE_NAME tev_uint64
#  include FIELD_TYPE_FILE
#endif

#if VALUE_SIZE >= 4
#  define FIELD_TYPE_TYPE float
#  define FIELD_TYPE_NAME tev_float
#  include FIELD_TYPE_FILE
#endif

#if VALUE_SIZE >= 8
#  define FIELD_TYPE_TYPE double
#  define FIELD_TYPE_NAME tev_double
#  include FIELD_TYPE_FILE
#endif

#if defined(TERARK_C_LONG_DOUBLE_SIZE) && VALUE_SIZE >= TERARK_C_LONG_DOUBLE_SIZE
#  define FIELD_TYPE_TYPE long double
#  define FIELD_TYPE_NAME tev_ldouble
#  include FIELD_TYPE_FILE
#endif

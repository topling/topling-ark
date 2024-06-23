// input MACRO params:
//   MAKE_FUNC_NAME

#ifndef VALUE_SIZE
//# error must define VALUE_SIZE, if you need not VALUE_SIZE, define it as TERARK_C_MAX_VALUE_SIZE
#endif

	MAKE_FUNC_NAME(tev_char),
	MAKE_FUNC_NAME(tev_byte),

#if !defined(VALUE_SIZE) || defined(VALUE_SIZE) && VALUE_SIZE >= 2
	MAKE_FUNC_NAME(tev_int16),
	MAKE_FUNC_NAME(tev_uint16),
#else
	NULL, NULL,
#endif

#if !defined(VALUE_SIZE) || defined(VALUE_SIZE) && VALUE_SIZE >= 4
	MAKE_FUNC_NAME(tev_int32),
	MAKE_FUNC_NAME(tev_uint32),
#else
	NULL, NULL,
#endif

#if !defined(VALUE_SIZE) || defined(VALUE_SIZE) && VALUE_SIZE >= 8
	MAKE_FUNC_NAME(tev_int64),
	MAKE_FUNC_NAME(tev_uint64),
#else
	NULL, NULL,
#endif

#if !defined(VALUE_SIZE) || defined(VALUE_SIZE) && VALUE_SIZE >= 4
	MAKE_FUNC_NAME(tev_float),
#else
	NULL,
#endif

#if !defined(VALUE_SIZE) || defined(VALUE_SIZE) && VALUE_SIZE >= 8
	MAKE_FUNC_NAME(tev_double),
#else
	NULL,
#endif

#ifdef TERARK_C_LONG_DOUBLE_SIZE
#  if !defined(VALUE_SIZE) || defined(VALUE_SIZE) && VALUE_SIZE >= TERARK_C_LONG_DOUBLE_SIZE
	MAKE_FUNC_NAME(tev_ldouble),
#else
	NULL,
#endif
#endif

#undef VALUE_SIZE
#undef FAST_ELEM_SIZE

// input MACRO params:
//   MAKE_FUNC_NAME

	MAKE_FUNC_NAME(tev_char),
	MAKE_FUNC_NAME(tev_byte),

	MAKE_FUNC_NAME(tev_int16),
	MAKE_FUNC_NAME(tev_uint16),

	MAKE_FUNC_NAME(tev_int32),
	MAKE_FUNC_NAME(tev_uint32),

	MAKE_FUNC_NAME(tev_int64),
	MAKE_FUNC_NAME(tev_uint64),

	MAKE_FUNC_NAME(tev_float),

	MAKE_FUNC_NAME(tev_double),

#ifdef TERARK_C_LONG_DOUBLE_SIZE
	MAKE_FUNC_NAME(tev_ldouble),
#endif

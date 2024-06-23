// input MACRO params:
//   FIELD_TYPE_TYPE
//   FIELD_TYPE_NAME
//   FIELD_TYPE_PROMOTED [optional]
//   FAST_ELEM_SIZE expand to elem_size or sizeof(char*)
//   GET_KEY

#ifndef FIELD_TYPE_PROMOTED
#  define FIELD_TYPE_PROMOTED FIELD_TYPE_TYPE
#endif

ptrdiff_t _TM_lower_bound(const char* _First, ptrdiff_t _Count, ptrdiff_t elem_size, ptrdiff_t field_offset, va_list _vaKey)
{
	FIELD_TYPE_PROMOTED _Key = va_arg(_vaKey, FIELD_TYPE_PROMOTED);
	ptrdiff_t lo = 0, hi = _Count;
	while (lo < hi)	{
		ptrdiff_t mid = (lo + hi) / 2;
		const char* _Mid = _First + FAST_ELEM_SIZE * mid;
		if (GET_KEY(_Mid) < _Key)
			lo = mid + 1;
		else
			hi = mid;
	}
	return lo;
}

ptrdiff_t _TM_upper_bound(const char* _First, ptrdiff_t _Count, ptrdiff_t elem_size, ptrdiff_t field_offset, va_list _vaKey)
{
	FIELD_TYPE_PROMOTED _Key = va_arg(_vaKey, FIELD_TYPE_PROMOTED);
	ptrdiff_t lo = 0, hi = _Count;
	while (lo < hi)	{
		ptrdiff_t mid = (lo + hi) / 2;
		const char* _Mid = _First + FAST_ELEM_SIZE * mid;
		if (GET_KEY(_Mid) <= _Key)
			lo = mid + 1;
		else
			hi = mid;
	}
	return lo;
}

ptrdiff_t _TM_equal_range(const char* _First, ptrdiff_t _Count, ptrdiff_t elem_size, ptrdiff_t field_offset, ptrdiff_t* upp, va_list _vaKey)
{
	FIELD_TYPE_PROMOTED _Key = va_arg(_vaKey, FIELD_TYPE_PROMOTED);
	ptrdiff_t lo = 0, hi = _Count;
	while (lo < hi)	{
		ptrdiff_t mid = (lo + hi) / 2;
		const char* _Mid = _First + FAST_ELEM_SIZE * mid;
		const FIELD_TYPE_PROMOTED _KeyMid = GET_KEY(_Mid);
		if (_KeyMid < _Key)
			lo = mid + 1; // range begins above _Mid, loop
		else if (_Key < _KeyMid)
			hi = mid; // range in first half, loop
		else
		{	// range straddles mid, find each end and return

		//  GCC 在处理 va_list copy 时有问题,
		//      使用 va_copy 不行，在参数表中重复两个 va_list 参数也不行。
		//      所以，将 _TM_lower_bound/_TM_upper_bound 手工展开
		//  GCC has bugs when deal with va_list copy,
		//      bugs both present on va_copy and copy by duplicate on param-list.
		//      so expand _TM_lower_bound/_TM_upper_bound manually.

			// upper_bound
			{
				ptrdiff_t ulo = mid + 1, uhi = hi;
				while (ulo < uhi) {
					ptrdiff_t umid = (ulo + uhi) / 2;
					const char* _MidU = _First + FAST_ELEM_SIZE * umid;
					if (GET_KEY(_MidU) <= _Key)
						ulo = umid + 1;
					else
						uhi = umid;
				}
				*upp = ulo;
			}
			// lower_bound....
			{
				ptrdiff_t llo = lo, lhi = mid;
				while (llo < lhi) {
					ptrdiff_t lmid = (llo + lhi) / 2;
					const char* _MidL = _First + FAST_ELEM_SIZE * lmid;
					if (GET_KEY(_MidL) < _Key)
						llo = lmid + 1;
					else
						lhi = lmid;
				}
				return llo;
			}
		}
	}
	return *upp = lo; // empty range
}

ptrdiff_t _TM_binary_search(const char* _First, ptrdiff_t _Count, ptrdiff_t elem_size, ptrdiff_t field_offset, va_list _vaKey)
{
//	ptrdiff_t lb = _TM_lower_bound(_First0, _Count0, FAST_ELEM_SIZE, field_offset, _vaKey);
//	return lb != _Count0 && GET_KEY(_First0) == *(FIELD_TYPE_PROMOTED*)_vaKey;
	const char* _Mid = NULL;
	FIELD_TYPE_PROMOTED _Key = va_arg(_vaKey, FIELD_TYPE_PROMOTED);
	ptrdiff_t lo = 0, hi = _Count;
	while (lo < hi)	{
		ptrdiff_t mid = (lo + hi) / 2;
		_Mid = _First + FAST_ELEM_SIZE * mid;
		if (GET_KEY(_Mid) < _Key)
			lo = mid + 1;
		else
			hi = mid;
	}
	return lo != _Count && _Key == GET_KEY(_Mid);
}

#undef FIELD_TYPE_TYPE
#undef FIELD_TYPE_PROMOTED
#undef FIELD_TYPE_NAME


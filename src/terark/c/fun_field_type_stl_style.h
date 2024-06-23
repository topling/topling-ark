// input MACRO params:
//   FIELD_TYPE_TYPE
//   FIELD_TYPE_NAME
//   FIELD_TYPE_PROMOTED [optional]
//   FAST_ELEM_SIZE

#ifndef FIELD_TYPE_PROMOTED
#  define FIELD_TYPE_PROMOTED FIELD_TYPE_TYPE
#endif

ptrdiff_t _TM_lower_bound(const char* _First0, ptrdiff_t _Count, ptrdiff_t elem_size, ptrdiff_t field_offset, va_list _vaKey)
{
	const char* _First = _First0;
	FIELD_TYPE_PROMOTED _Key = va_arg(_vaKey, FIELD_TYPE_PROMOTED);

	for (; 0 < _Count; )
	{	// divide and conquer, find half that contains answer
		ptrdiff_t _Count2 = _Count / 2;
		const char* _Mid = _First + FAST_ELEM_SIZE * _Count2;
		if (GET_KEY(_Mid) < _Key)
			_First = _Mid + FAST_ELEM_SIZE, _Count -= _Count2 + 1;
		else
			_Count = _Count2;
	}
	return (_First - _First0) / FAST_ELEM_SIZE;
}

ptrdiff_t _TM_upper_bound(const char* _First0, ptrdiff_t _Count, ptrdiff_t elem_size, ptrdiff_t field_offset, va_list _vaKey)
{
	const char* _First = _First0;
	FIELD_TYPE_PROMOTED _Key = va_arg(_vaKey, FIELD_TYPE_PROMOTED);

	for (; 0 < _Count; )
	{	// divide and conquer, find half that contains answer
		ptrdiff_t _Count2 = _Count / 2;
		const char* _Mid = _First + FAST_ELEM_SIZE * _Count2;
		if (_Key >= GET_KEY(_Mid))
			_First = _Mid + FAST_ELEM_SIZE, _Count -= _Count2 + 1;
		else
			_Count = _Count2;
	}
	return (_First - _First0) / FAST_ELEM_SIZE;
}

ptrdiff_t _TM_equal_range(const char* _First0, ptrdiff_t _Count, ptrdiff_t elem_size, ptrdiff_t field_offset, ptrdiff_t* upp, va_list _vaKey)
{
	FIELD_TYPE_PROMOTED _Key = va_arg(_vaKey, FIELD_TYPE_PROMOTED);
	const char* _First = _First0;
	for (; 0 < _Count; )
	{	// divide and conquer, check midpoint
		ptrdiff_t _Count2 = _Count / 2;
		const char* _Mid = _First + FAST_ELEM_SIZE * _Count2;

		const FIELD_TYPE_PROMOTED _KeyMid = GET_KEY(_Mid);
		if (_KeyMid < _Key)
		{	// range begins above _Mid, loop
			_First = _Mid + FAST_ELEM_SIZE;
			_Count -= _Count2 + 1;
		}
		else if (_Key < _KeyMid)
			_Count = _Count2;	// range in first half, loop
		else
		{	// range straddles mid, find each end and return

		//  GCC 在处理 va_list copy 时有问题,
		//      使用 va_copy 不行，在参数表中重复两个 va_list 参数也不行。
		//      所以，将 _TM_lower_bound/_TM_upper_bound 手工展开
		//  GCC has bugs when deal with va_list copy,
		//      bugs both present on va_copy and copy by duplicate on param-list.
		//      so expand _TM_lower_bound/_TM_upper_bound manually.
		//
		//	ptrdiff_t _First2 = _TM_lower_bound(_First, _Count2, FAST_ELEM_SIZE, field_offset, _vaKeyBak);
		//	ptrdiff_t _Last2  = _TM_upper_bound(_Mid + FAST_ELEM_SIZE, _Count - (_Count2 + 1), FAST_ELEM_SIZE, field_offset, _vaKeyBak);
		//	*upp = (_Mid - _First0) / FAST_ELEM_SIZE + _Last2 + 1;
		//	return (_First - _First0) / FAST_ELEM_SIZE + _First2;

			// upper_bound
			{
				const char* _FirstU = _Mid + FAST_ELEM_SIZE;
				ptrdiff_t _CountU = _Count - (_Count2 + 1);
				for (; 0 < _CountU; )
				{	// divide and conquer, find half that contains answer
					ptrdiff_t _Count2U = _CountU / 2;
					const char* _MidU = _FirstU + FAST_ELEM_SIZE * _Count2U;
					if (_Key >= GET_KEY(_MidU))
						_FirstU = _MidU + FAST_ELEM_SIZE, _CountU -= _Count2U + 1;
					else
						_CountU = _Count2U;
				}
				*upp = (_FirstU - _First0) / FAST_ELEM_SIZE;
			}
			// lower_bound....
			{
				const char* _FirstL = _First;
				ptrdiff_t _CountL = _Count2;
				for (; 0 < _CountL; )
				{	// divide and conquer, find half that contains answer
					ptrdiff_t _Count2L = _CountL / 2;
					const char* _MidL = _FirstL + FAST_ELEM_SIZE * _Count2L;
					if (GET_KEY(_MidL) < _Key)
						_FirstL = _MidL + FAST_ELEM_SIZE, _CountL -= _Count2L + 1;
					else
						_CountL = _Count2L;
				}
				return (_FirstL - _First0) / FAST_ELEM_SIZE;
			}
		}
	}
	return *upp = (_First - _First0) / FAST_ELEM_SIZE; // empty range
}

ptrdiff_t _TM_binary_search(const char* _First0, ptrdiff_t _Count0, ptrdiff_t elem_size, ptrdiff_t field_offset, va_list _vaKey)
{
//	ptrdiff_t lb = _TM_lower_bound(_First0, _Count0, FAST_ELEM_SIZE, field_offset, _vaKey);
//	return lb != _Count0 && GET_KEY(_First0) == *(FIELD_TYPE_PROMOTED*)_vaKey;
	ptrdiff_t _Count = _Count0;
	const char* _First = _First0;
	FIELD_TYPE_PROMOTED _Key = va_arg(_vaKey, FIELD_TYPE_PROMOTED);

	for (; 0 < _Count; )
	{	// divide and conquer, find half that contains answer
		ptrdiff_t _Count2 = _Count / 2;
		const char* _Mid = _First + FAST_ELEM_SIZE * _Count2;
		if (GET_KEY(_Mid) < _Key)
			_First = _Mid + FAST_ELEM_SIZE, _Count -= _Count2 + 1;
		else
			_Count = _Count2;
	}
	return _First0 + FAST_ELEM_SIZE * _Count0 != _First && _Key == GET_KEY(_First);
}

#undef FIELD_TYPE_TYPE
#undef FIELD_TYPE_PROMOTED
#undef FIELD_TYPE_NAME


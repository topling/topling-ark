// input MACRO params:
//   VALUE_SIZE
//	 FAST_ELEM_SIZE
//   FIELD_TYPE_TYPE
//   FIELD_TYPE_NAME

static void _Insertion_sort(char* _First, char* _Last, ptrdiff_t field_offset)
{	// insertion sort [_First, _Last), using LESS_THAN
	char* _Next = _First;
	assert(_Last > _First);
	assert(_Last - _First > (ptrdiff_t)(FAST_ELEM_SIZE));
	assert((_Last - _First) % FAST_ELEM_SIZE == 0);
	assert(sizeof(TempBuf) == FAST_ELEM_SIZE);
	while ((_Next += FAST_ELEM_SIZE) != _Last)
	{	// order next element
		DEFINE_FROM(_Val, _Next);

		if (VAL_LESS_THAN(_Next, _First))
		{	// found new earliest element, move to front
			CHECK_BOUND(_First + FAST_ELEM_SIZE);
			CHECK_BOUND(_Next - 1);
			{
				TempBuf* pEnd = (TempBuf*)_Next;
				TempBuf* pBeg = (TempBuf*)_First;
				TempBuf* pDst = (TempBuf*)_Next + 1;
				for (; pEnd > pBeg;)
					*--pDst = *--pEnd;
			}
			ASSIGN_FROM(_First, &_Val);
		}
		else
		{	// look for insertion point after first
			char* _First1 = _Next;
			char* _Next1 = _Next;
			for (; VAL_LESS_THAN(&_Val, _First1 -= FAST_ELEM_SIZE); _Next1 = _First1)
			{
				ASSIGN_FROM(_Next1, _First1);	// move hole down
			}
			ASSIGN_FROM(_Next1, &_Val);	// insert element in hole
		}
	}
}

static void _Median(char* _First, char* _Mid, char* _Last, ptrdiff_t count, ptrdiff_t field_offset)
{	// sort median element to middle
//	FIELD_TYPE_TYPE* p = (FIELD_TYPE_TYPE*)_First;
	assert(_Last - _First + FAST_ELEM_SIZE == FAST_ELEM_SIZE * count);
	if (41 < count)
	{	// median of nine
		ptrdiff_t _Step = FAST_ELEM_SIZE * (count / 8);
// 		_Med3(_First, _First + _Step, _First + 2 * _Step, field_offset);
// 		_Med3(_Mid - _Step, _Mid, _Mid + _Step, field_offset);
// 		_Med3(_Last - 2 * _Step, _Last - _Step, _Last, field_offset);
// 		_Med3(_First + _Step, _Mid, _Last - _Step, field_offset);

		char *p1 = _First + _Step, *p2 = _First + 2 * _Step;
		_Med3(_First, p1, p2);

		p1 = _Mid - _Step; p2 = _Mid + _Step;
		_Med3(p1, _Mid, p2);

		p1 = _Last - 2 * _Step; p2 = _Last - _Step;
		_Med3(p1, p2, _Last);

		p1 = _First + _Step; p2 = _Last - _Step;
		_Med3(p1, _Mid, p2);
	}
	else
	{
	//	_Med3(_First, _Mid, _Last, field_offset);
		_Med3(_First, _Mid, _Last);
	}
}

void _Unguarded_partition(char* _First, char* _Last, ptrdiff_t count, ptrdiff_t field_offset, char* res[2])
{	// partition [_First, _Last), using VAL_LESS_THAN
//	FIELD_TYPE_TYPE* p = (FIELD_TYPE_TYPE*)_First;
	char* _Mid = _First + FAST_ELEM_SIZE * (count / 2);
	char* _Pfirst = _Mid;
	char* _Plast = _Pfirst + FAST_ELEM_SIZE;
	assert(_First + FAST_ELEM_SIZE * count == _Last);

	_Median(_First, _Mid, _Last - FAST_ELEM_SIZE, count, field_offset);

	while (_First < _Pfirst && VAL_EQUAL(_Pfirst - FAST_ELEM_SIZE, _Pfirst))
		_Pfirst -= FAST_ELEM_SIZE;
	while (_Plast < _Last && VAL_EQUAL(_Plast, _Pfirst))
		_Plast += FAST_ELEM_SIZE;

  { // make a block
	char* _Gfirst = _Plast;
	char* _Glast = _Pfirst;

	for (; ; )
	{	// partition
		for (; _Gfirst < _Last; _Gfirst += FAST_ELEM_SIZE)
		{
			if (VAL_LESS_THAN(_Pfirst, _Gfirst))
				;
			else if (VAL_LESS_THAN(_Gfirst, _Pfirst))
				break;
			else {
				CHECKED_SWAP(_Plast, _Gfirst);
				_Plast += FAST_ELEM_SIZE;
			}
		}
		for (; _First < _Glast; _Glast -= FAST_ELEM_SIZE)
		{
			if (VAL_LESS_THAN(_Glast - FAST_ELEM_SIZE, _Pfirst))
				;
			else if (VAL_LESS_THAN(_Pfirst, _Glast - FAST_ELEM_SIZE))
				break;
			else {
				_Pfirst -= FAST_ELEM_SIZE;
				CHECKED_SWAP(_Pfirst, _Glast - FAST_ELEM_SIZE);
			}
		}
		if (_Glast == _First && _Gfirst == _Last)
		{
			res[0] = _Pfirst;
			res[1] = _Plast;
			return;
		}

		if (_Glast == _First)
		{	// no room at bottom, rotate pivot upward
			if (_Plast != _Gfirst)
				CHECKED_SWAP(_Pfirst, _Plast);
			CHECKED_SWAP(_Pfirst, _Gfirst);
			_Plast += FAST_ELEM_SIZE;
			_Pfirst += FAST_ELEM_SIZE;
			_Gfirst += FAST_ELEM_SIZE;
		}
		else if (_Gfirst == _Last)
		{	// no room at top, rotate pivot downward
			_Glast -= FAST_ELEM_SIZE;
			_Pfirst -= FAST_ELEM_SIZE;
			if (_Glast != _Pfirst)
				CHECKED_SWAP(_Glast, _Pfirst);
			_Plast -= FAST_ELEM_SIZE;
			CHECKED_SWAP(_Pfirst, _Plast);
		}
		else {
			_Glast -= FAST_ELEM_SIZE;
			CHECKED_SWAP(_Gfirst, _Glast);
			_Gfirst += FAST_ELEM_SIZE;
		}
	}
  }
}

void _Heap_up(char* _First, ptrdiff_t _Hole, ptrdiff_t _Top, ptrdiff_t field_offset, TempBuf* _Val)
{	// percolate _Hole to _Top or where _Val belongs, using operator<
	ptrdiff_t _Idx = (_Hole - 1) / 2;
	for (; _Top < _Hole && VAL_LESS_THAN(_First + FAST_ELEM_SIZE*_Idx, _Val);
		_Idx = (_Hole - 1) / 2)
	{	// move _Hole up to parent
		ASSIGN_FROM(_First + FAST_ELEM_SIZE*_Hole, _First + FAST_ELEM_SIZE*_Idx);
		_Hole = _Idx;
	}
	ASSIGN_FROM(_First + FAST_ELEM_SIZE*_Hole, _Val);	// drop _Val into final hole
}

void _TM_push_heap(char* _First, ptrdiff_t count, ptrdiff_t field_offset)
{	// push *(_Last - 1) onto heap at [_First, _Last - 1), using _Pred
	if (count > 1)
	{	// check and push to nontrivial heap
		char* _Last = _First + FAST_ELEM_SIZE * count;
		DEFINE_FROM(tmp, _Last - FAST_ELEM_SIZE);
		_Heap_up(_First, count-1, 0, field_offset, &tmp);
	}
}

void _Heap_down_up(char* _First, ptrdiff_t _Hole, ptrdiff_t _Bottom, ptrdiff_t field_offset, TempBuf* _Val)
{	// percolate _Hole to _Bottom, then push _Val, using _Pred
	ptrdiff_t _Top = _Hole;
	ptrdiff_t _Idx = 2 * _Hole + 2;

	for (; _Idx < _Bottom; _Idx = 2 * _Idx + 2)
	{	// move _Hole down to larger child
		if (VAL_LESS_THAN(_First + FAST_ELEM_SIZE*_Idx, _First + FAST_ELEM_SIZE*(_Idx - 1)))
			--_Idx;
		ASSIGN_FROM(_First + FAST_ELEM_SIZE*_Hole, _First + FAST_ELEM_SIZE*_Idx);
		_Hole = _Idx;
	}

	if (_Idx == _Bottom)
	{	// only child at bottom, move _Hole down to it
		ASSIGN_FROM(_First + FAST_ELEM_SIZE*_Hole, _First + FAST_ELEM_SIZE*(_Bottom - 1));
		_Hole = _Bottom - 1;
	}
	_Heap_up(_First, _Hole, _Top, field_offset, _Val);
}

void _TM_Heap_update(char* _First, ptrdiff_t _IdxUpdated, ptrdiff_t _Bottom, ptrdiff_t field_offset)
{
	DEFINE_FROM(_Val, _First + FAST_ELEM_SIZE * _IdxUpdated);
	_Heap_down_up(_First, _IdxUpdated, _Bottom, field_offset, &_Val);
}

void _TM_pop_heap(char* _First, ptrdiff_t count, ptrdiff_t field_offset)
{
	if (count > 1)
	{
		char* _Last = _First + FAST_ELEM_SIZE * count;
		DEFINE_FROM(_Val, _Last - FAST_ELEM_SIZE);
		ASSIGN_FROM(_Last - FAST_ELEM_SIZE, _First);
		_Heap_down_up(_First, 0, count-1, field_offset, &_Val);
	}
}

void _TM_make_heap(char* _First, ptrdiff_t count, ptrdiff_t field_offset)
{	// make nontrivial [_First, _Last) into a heap, using operator<
	ptrdiff_t _Bottom = count;
	ptrdiff_t _Hole = _Bottom / 2;

	for (; 0 < _Hole; )
	{	// reheap top half, bottom to top
	  --_Hole;
	  { // make scope
		DEFINE_FROM(_Val, _First + FAST_ELEM_SIZE * _Hole);
		_Heap_down_up(_First, _Hole, _Bottom, field_offset, &_Val);
	  }
	}
}

void _TM_sort_heap(char* _First, ptrdiff_t count, ptrdiff_t field_offset)
{	// order heap by repeatedly popping, using _Pred
	for (; 1 < count; --count)
	{
	//	_TM_pop_heap(_First, count, field_offset);
	//
	//  manual inline _TM_pop_heap
		char* _Last = _First + FAST_ELEM_SIZE * count;
		DEFINE_FROM(_Val, _Last - FAST_ELEM_SIZE);
		ASSIGN_FROM(_Last - FAST_ELEM_SIZE, _First);
		_Heap_down_up(_First, 0, count-1, field_offset, &_Val);
	}
}

void _TM_sort_loop(char* _First, char* _Last, ptrdiff_t _Ideal, ptrdiff_t field_offset)
{	// order [_First, _Last), using _Pred
	ptrdiff_t _Count;
	assert(sizeof(TempBuf) == FAST_ELEM_SIZE);
	for (; C_ISORT_MAX < (_Count = (_Last - _First) / FAST_ELEM_SIZE) && 0 < _Ideal; )
	{	// divide and conquer by quicksort
		char* _Mid[2];
		_Unguarded_partition(_First, _Last, _Count, field_offset, _Mid);
		_Ideal /= 2, _Ideal += _Ideal / 2;	// allow 1.5 log2(N) divisions

		if (_Mid[0] - _First < _Last - _Mid[1])
		{	// loop on second half
			_TM_sort_loop(_First, _Mid[0], _Ideal, field_offset);
			_First = _Mid[1];
		}
		else
		{	// loop on first half
			_TM_sort_loop(_Mid[1], _Last, _Ideal, field_offset);
			_Last = _Mid[0];
		}
	}

	if (C_ISORT_MAX < _Count)
	{	// heap sort if too many divisions
		_TM_make_heap(_First, _Count, field_offset);
		_TM_sort_heap(_First, _Count, field_offset);
	}
	else if (1 < _Count)
	{
		_Insertion_sort(_First, _Last, field_offset); // small
	}
}

void _TM_sort(char* _First, ptrdiff_t count, ptrdiff_t field_offset)
{
	_TM_sort_loop(_First, _First + VALUE_SIZE * count, count, field_offset);
}

#undef FIELD_TYPE_NAME
#undef FIELD_TYPE_TYPE


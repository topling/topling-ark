/* vim: set tabstop=4 : */
#include "bitvector.hpp"
#include <assert.h>
#include "io/byte_swap.hpp"

namespace terark {

// mask1[i] 的右边 i 位是 0, 左边 32-i 位是 1
static const int mask1[] = {
	0xFFFFFFFF,0xFFFFFFFE,0xFFFFFFFC,0xFFFFFFF8,
	0xFFFFFFF0,0xFFFFFFE0,0xFFFFFFC0,0xFFFFFF80,
	0xFFFFFF00,0xFFFFFE00,0xFFFFFC00,0xFFFFF800,
	0xFFFFF000,0xFFFFE000,0xFFFFC000,0xFFFF8000,
	0xFFFF0000,0xFFFE0000,0xFFFC0000,0xFFF80000,
	0xFFF00000,0xFFE00000,0xFFC00000,0xFF800000,
	0xFF000000,0xFE000000,0xFC000000,0xF8000000,
	0xF0000000,0xE0000000,0xC0000000,0x80000000
};

// mask2[i] 的右边 i 位是 1, 左边 32-i 位是 0
static const int mask2[] = {
	0x00000000,0x00000001,0x00000003,0x00000007,
	0x0000000F,0x0000001F,0x0000003F,0x0000007F,
	0x000000FF,0x000001FF,0x000003FF,0x000007FF,
	0x00000FFF,0x00001FFF,0x00003FFF,0x00007FFF,
	0x0000FFFF,0x0001FFFF,0x0003FFFF,0x0007FFFF,
	0x000FFFFF,0x001FFFFF,0x003FFFFF,0x007FFFFF,
	0x00FFFFFF,0x01FFFFFF,0x03FFFFFF,0x07FFFFFF,
	0x0FFFFFFF,0x1FFFFFFF,0x3FFFFFFF,0x7FFFFFFF
};

bool
all_true(const std::vector<unsigned int>& vec, int lower, int upper)
{
	assert(lower >= 0);
	assert(lower <= upper);

	int il = lower / 32, bl = lower % 32;
	int iu = upper / 32, bu = upper % 32;
	if (il == iu)
	{
		unsigned int f = mask1[bl] & mask2[bu];
		if ((vec[il] & f) == f)
			return true;
		else
			return false;
	}
	if ((vec[il] & mask1[bl]) != mask1[bl])
		return false;

	for (++il; il != iu; ++il)
		if (0xFFFFFFFF != vec[il])
			return false;

	// 必须加上 0!=bu 的判断，因为 upper 有可能是 bit 的上界
	if (0 != bu && (vec[iu] & mask2[bu]) != mask2[bu])
		return false;

	return true;
}

bool
any_true(const std::vector<unsigned int>& vec, int lower, int upper)
{
	assert(lower >= 0);
	assert(lower <= upper);

	int il = lower / 32, bl = lower % 32;
	int iu = upper / 32, bu = upper % 32;
	if (il == iu)
	{
		unsigned int f = mask1[bl] & mask2[bu];
		if (vec[il] & f)
			return true;
		else
			return false;
	}
	if (vec[il] & mask1[bl])
		return true;

	for (++il; il != iu; ++il)
		if (vec[il])
			return true;

	// 必须加上 0!=bu 的判断，因为 upper 有可能是 bit 的上界
	if (0 != bu && vec[iu] & mask2[bu])
		return true;

	return false;
}

bool
all_false(const std::vector<unsigned int>& vec, int lower, int upper)
{
	assert(lower >= 0);
	assert(lower <= upper);

	int il = lower / 32, bl = lower % 32;
	int iu = upper / 32, bu = upper % 32;
	if (il == iu)
	{
		unsigned int f = mask1[bl] & mask2[bu];
		if ((vec[il] & f) == 0)
			return true;
		else
			return false;
	}
	if (vec[il] & mask1[bl])
		return false;

	for (++il; il != iu; ++il)
		if (0 != vec[il])
			return false;

	// 必须加上 0!=bu 的判断，因为 upper 有可能是 bit 的上界
	if (0 != bu && vec[iu] & mask2[bu])
		return false;

	return true;
}

void
bvec_set(std::vector<unsigned int>& vec, int lower, int upper)
{
	assert(lower >= 0);
	assert(lower <= upper);

	int il = lower / 32, bl = lower % 32;
	int iu = upper / 32, bu = upper % 32;
	if (il == iu)
	{
		unsigned int f = mask1[bl] & mask2[bu];
		vec[il] |= f;
		return;
	}
	vec[il] |= mask1[bl];

	for (++il; il != iu; ++il)
		vec[il] = 0xFFFFFFFF;

	if (0 != bu)
		vec[iu] |= mask2[bu];
}

void
bvec_reset(std::vector<unsigned int>& vec, int lower, int upper)
{
	assert(lower >= 0);
	assert(lower <= upper);

	int il = lower / 32, bl = lower % 32;
	int iu = upper / 32, bu = upper % 32;
	if (il == iu)
	{
		unsigned int f = mask1[bl] & mask2[bu];
		vec[il] &= ~f;
		return;
	}
	vec[il] &= ~mask1[bl];

	for (++il; il != iu; ++il)
		vec[il] = 0;

	if (0 != bu)
		vec[iu] &= ~mask2[bu];
}


} // namespace terark


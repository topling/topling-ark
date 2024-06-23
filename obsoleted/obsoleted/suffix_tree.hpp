#ifndef __terark_suffix_tree__
#define __terark_suffix_tree__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
# pragma warning(disable: 4127)
#endif

#include <terark/config.hpp>
#include <stddef.h>
#include <vector>

namespace terark {

// this is the fast version, the worst complex is O(n*logn*logn) time
// the average is O(n*logx(n)), logx(n)=log(n)+log(log(n))+...
class suffix_array_impl;
class TERARK_DLL_EXPORT suffix_array
{
	suffix_array_impl* impl;

// disable:
	suffix_array(const suffix_array& y);
	suffix_array& operator=(const suffix_array& y);

public:
	suffix_array();
	~suffix_array();
	void append(int ch);
	const std::vector<int>& build();
	const std::vector<int>& build_n(int FirstN);
	const std::vector<int>& build_o(int FirstN);
};
/*
class suffix_tree
{
public:
	struct node
	{
		int pos;
		std::map<int, node> sub;
	};

	void build();
protected:
	std::map<int, node> root;
	const int* base;
	int nlen;
};
*/
} // namespace terark

#endif // __terark_suffix_tree__


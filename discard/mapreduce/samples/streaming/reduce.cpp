#include "Reducer.h"
#include <terark/trb_cxx.hpp>
/*
#ifdef _MSC_VER
#include <unordered_map>
#else
#include <ext/hash_map>
#endif

struct Hash_cs
{
	size_t operator()(const cs_t& cs) const
	{
		size_t h = 1089759;
		for (ptrdiff_t i = 0, n = cs.size(); i < n; ++i)
			h = ( (h >> (sizeof(size_t)-3) | h << 3) | cs[i] ) + 37981 ;
		return h;
	}
	// equal
	bool operator()(const cs_t& x, const cs_t& y) const
	{
		if (x.size() != y.size()) return false;
		return memcmp(x.begin(), y.begin(), x.size() * sizeof(CharT)) == 0;
	}
};
#ifdef _MSC_VER
	typedef std::tr1::unordered_map<cs_t, unsigned, Hash_cs, Hash_cs> tw_map_t;
#else
	typedef __gnu_cxx::hash_map<cs_t, unsigned, Hash_cs, Hash_cs> tw_map_t;
//	typedef tr1::unordered_map<cs_t, unsigned, Hash_cs, Hash_cs> tw_map_t;
#endif
*/

class TextReducer : public Reducer
{
//	typedef trbstrmap<AutoGrownMemIO>  map_t;
//	map_t kvm;
public:
    virtual void reduce(const void* key, size_t klen, const void* val, size_t vlen)
	{
		printf("%.*s\t%.*s\n", (int)klen, key, (int)vlen, val);
	}
};

int main(int argc, char* argv[])
{
	ReduceContext context(argc, argv);
	TextReducer reducer;
	reducer.run(context);
	return 0;
}


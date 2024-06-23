/* vim: set tabstop=4 : */
#include "trb_cxx.hpp"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <algorithm>

extern "C" {
int
trb_vtab_compare(const struct trb_vtab* vtab,
				 const struct trb_tree* tree,
				 const void* x,
				 const void* y)
{
    const struct trb_vtab *tx = (const struct trb_vtab*)x;
    const struct trb_vtab *ty = (const struct trb_vtab*)y;

    if (tx->key_offset < ty->key_offset) return -1;
    if (tx->key_offset > ty->key_offset) return +1;
    if (tx->key_size < ty->key_size) return -1;
    if (tx->key_size > ty->key_size) return +1;

    if (tx->data_offset < ty->data_offset) return -1;
    if (tx->data_offset > ty->data_offset) return +1;
    if (tx->data_size < ty->data_size) return -1;
    if (tx->data_size > ty->data_size) return +1;

	if ((void*)tx->pf_compare < (void*)ty->pf_compare) return -1;
	if ((void*)tx->pf_compare > (void*)ty->pf_compare) return +1;

    return 0;
}

int
trb_compare_tev_inline_cstr(
					 const struct trb_vtab* vtab,
					 const struct trb_tree* tree,
					 const void* x,
					 const void* y)
{
	return strcmp((char*)x, (char*)y);
}

int
trb_compare_tev_inline_cstr_nocase(
					 const struct trb_vtab* vtab,
					 const struct trb_tree* tree,
					 const void* x,
					 const void* y)
{
#ifdef _MSC_VER
	return _stricmp((char*)x, (char*)y);
#else
	return strcasecmp((char*)x, (char*)y);
#endif
}

int
trb_compare_tev_std_string( const struct trb_vtab* vtab,
							const struct trb_tree* tree,
							const void* x,
							const void* y)
{
	const std::string *tx = (const std::string*)x;
    const std::string *ty = (const std::string*)y;
	return tx->compare(*ty);
}

int
trb_compare_tev_std_wstring(const struct trb_vtab* vtab,
							const struct trb_tree* tree,
							const void* x,
							const void* y)
{
	const std::wstring *tx = (const std::wstring*)x;
    const std::wstring *ty = (const std::wstring*)y;
	return tx->compare(*ty);
}

int
trb_compare_tev_std_string_nocase(
		const struct trb_vtab* vtab,
		const struct trb_tree* tree,
        const void* x,
        const void* y)
{
	using namespace std; // for min
	const std::string *tx = (const std::string*)x;
    const std::string *ty = (const std::string*)y;
#ifdef _MSC_VER
	return _strnicmp
#else
	return strncasecmp
#endif
	(tx->c_str(), ty->c_str(), min(tx->size(), ty->size()));
}

int
trb_compare_tev_std_wstring_nocase(
		const struct trb_vtab* vtab,
		const struct trb_tree* tree,
        const void* x,
        const void* y)
{
	using namespace std; // for min
	const std::wstring *tx = (const std::wstring*)x;
    const std::wstring *ty = (const std::wstring*)y;
#ifdef _MSC_VER
	return _wcsnicmp
#else
	return wcsncasecmp
#endif
	(tx->c_str(), ty->c_str(), min(tx->size(), ty->size()));
}

} // extern "C"

namespace terark {

void trbxx_vtab_init_by_cxx_type(struct trb_vtab* vtab
								 , field_type_t ikey_type
								 , trb_func_compare key_comp
								 , trb_func_compare buildin_key_comp)
{
	if (NULL == key_comp) {
		assert(NULL == vtab->pf_compare);
		vtab->pf_compare = buildin_key_comp;
	} else
		vtab->pf_compare = key_comp;

	// look C++ types as tev_app
	trb_vtab_init(vtab, tev_app);

	// set C++ types as C++ type itself
	vtab->key_type = ikey_type;
}

static
size_t
trb_tev_inline_cstr_data_get_size1(const struct trb_vtab* vtab, const void* data)
{
	size_t klen = *(unsigned char*)((char*)data + vtab->data_size) + 1; // 1 byte len
	size_t alen = klen + vtab->data_size;
	assert(klen <= 256+1);
	return alen;
}

static
size_t
trb_tev_inline_cstr_data_get_size2(const struct trb_vtab* vtab, const void* data)
{
	size_t klen = *(unsigned short*)((char*)data + vtab->data_size) + 2; // 2 byte len
	size_t alen = klen + vtab->data_size;
	assert(klen <= 256*256+2);
	return alen;
}

static
size_t
trb_tev_inline_cstr_data_get_size4(const struct trb_vtab* vtab, const void* data)
{
	size_t klen = *(int*)((char*)data + vtab->data_size) + 4; // 4 byte len
	size_t alen = klen + vtab->data_size;
	assert(klen < INT_MAX);
	return alen;
}

void trbxx_vtab_init(struct trb_vtab* vtab, field_type_t ikey_type, trb_func_compare key_comp)
{
	switch (ikey_type)
	{
	default:
		vtab->pf_compare = key_comp;
		trb_vtab_init(vtab, ikey_type);
		break;
	case tev_app:
		assert(NULL != key_comp);
		vtab->pf_compare = key_comp;
		trb_vtab_init(vtab, ikey_type);
		break;
	case tev_inline_cstr:
		switch (vtab->key_size)
		{
		default: assert(0); break;
		case 1: vtab->pf_data_get_size = &trb_tev_inline_cstr_data_get_size1; break;
		case 2: vtab->pf_data_get_size = &trb_tev_inline_cstr_data_get_size2; break;
		case 4: vtab->pf_data_get_size = &trb_tev_inline_cstr_data_get_size4; break;
		}
		trbxx_vtab_init_by_cxx_type(vtab, ikey_type, key_comp, &trb_compare_tev_inline_cstr);
		break;
	case tev_std_string:
		trbxx_vtab_init_by_cxx_type(vtab, ikey_type, key_comp, &trb_compare_tev_std_string);
		break;
	case tev_std_wstring:
		trbxx_vtab_init_by_cxx_type(vtab, ikey_type, key_comp, &trb_compare_tev_std_wstring);
		break;
	}
}

trb_node* trbxx_checked_alloc(const trb_vtab& vtab, size_t size, const char* msg)
{
	void* ptr = vtab.alloc->salloc(vtab.alloc, size);
	if (terark_unlikely(NULL == ptr)) {
#if defined(_MSC_VER) && _MSC_VER < 1600
		throw std::bad_alloc(msg);
#else
		throw std::bad_alloc();
#endif
	}
	return (trb_node*)ptr;
}

void ___trbtab_instantiate_templates()
{
//	typedef trbtab<int, std::pair<int,int> > trbtab_t;
	typedef trbmap<int,int> trbtab_t;
//	trbtab_t thin(tev_int);
	trbtab_t thin;
//	trbtab<int,int> thin(tev_int);
	thin.insert(std::make_pair(10, 10));
	thin.find(100);
	thin.lower_bound(100);
	thin.upper_bound(100);
	thin.equal_range(100);
	thin.erase(100);

	for (trbtab_t::iterator iter = thin.begin(); iter != thin.end(); ++iter)
	{
		(void)iter->first;
		(void)iter->second;
	}
	thin.erase(thin.begin());
	thin.erase(100);
	thin[100] = 200;

	trbstrmap<int> smap;
	smap["1"] = 1;
	smap["2"] = 2;
	std::string three("3");
	smap[three] = 3;
	smap.insert("4", 4);
	smap.insert("5", 2, 5);
	smap.lower_bound("2", 2);
	smap.upper_bound("2", 2);
	smap.equal_range("2", 2);
}

} // name space terark

#ifndef __terark_rockeet_index_Term_h__
#define __terark_rockeet_index_Term_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
# pragma warning(disable: 4127)
#endif

#include <new>
#include <string>
#include <vector>
#include <map>
#include <typeinfo>
#include <terark/stdtypes.hpp>
#include <terark/c/field_type.h>
#include <string.h>
#include <stdlib.h>

namespace terark { namespace rockeet {

struct TermInfo
{
	const void*  pKey;
	const void*  pVal;
	size_t		 cKey;
	size_t		 cVal;
	field_type_t keyType;
};

class TERARK_DLL_EXPORT ITerm
{
protected:
//private:
	virtual ~ITerm();

public:
	virtual void getInfo(TermInfo* info) const = 0;
	virtual void destroy() = 0;

public:
	/**
	 * @param[in] value value associated with key in this Document
	 *   -# normally is one byte for term freq
	 *   -# maybe nothing, this case valuelen == 0
	 *   -# has payload, such as term position
	 *   -# value is looked as raw binary data
	 */
	template<class Key>
	static ITerm* create(const Key& key, const void* pValue, size_t valuelen);

	template<class Key>
	static ITerm* createTermByteFreq(const Key& key, int freq);
};

template<class Key>
class VarValueTerm : public ITerm
{
public:
	Key key;
	size_t valuelen;

	VarValueTerm(const Key& key, size_t valuelen) : key(key), valuelen(valuelen) {}

	static size_t getKeySize(const Key&  x)	{ return sizeof(Key); }
	static size_t getKeySize(const char* x)	{ return strlen(x) + 1; }
	static size_t getKeySize(const std::string& x)	{ return x.size() + 1; }

	virtual void getInfo(TermInfo* info) const
	{
		info->pKey = &key;
		info->cKey = getKeySize(key);
		info->pVal = this + 1;
		info->cVal = valuelen;
		info->keyType = terark_get_field_type((Key*)0);
	}

	virtual void destroy() { assert(this); this->key.~Key(); free(this); }
};

template<class Key>
ITerm* ITerm::create(const Key& key, const void* pValue, size_t valuelen)
{
	VarValueTerm<Key>* p = (VarValueTerm<Key>*)malloc(sizeof(VarValueTerm<Key>) + valuelen);
	new(p)VarValueTerm<Key>(key, valuelen);
	memcpy(p + 1, pValue, valuelen);
	return p;
}
template<class Key>
ITerm* ITerm::createTermByteFreq(const Key& key, int freq)
{
	VarValueTerm<Key>* p = (VarValueTerm<Key>*)malloc(sizeof(VarValueTerm<Key>) + 1);
	new(p)VarValueTerm<Key>(key, 1);
	*(byte*)(p + 1) = (freq >= 255) ? 255 : freq;
	return p;
}

class TERARK_DLL_EXPORT TermVec : public std::vector<ITerm*>
{
public:
	// caller need ensure that it has no same terms in same TermVec
	// if it has same terms in same TermVec, it will cause error when append to fieldIndex!
	void push_back(ITerm* term);
	void clear();
};

class TERARK_DLL_EXPORT DocTermVec
//		: public RefCounter, public std::map<std::string, TermVec>
	: public std::map<std::string, TermVec>
{
private:
	// no copyable
	DocTermVec(const DocTermVec& rhs);
	DocTermVec& operator=(const DocTermVec& rhs);

	typedef std::map<std::string, TermVec> super;

public:
	DocTermVec();
	~DocTermVec();

	void erase(iterator iter);

	template<class Key>
	void addField(const std::string& fieldname, const Key& key, const void* pValue, size_t valuelen)
	{
		(*this)[fieldname].push_back(ITerm::create(key, pValue, valuelen));
	}
	template<class Key>
	void addField(const std::string& fieldname, const Key& key)
	{
		(*this)[fieldname].push_back(ITerm::create(key, NULL, 0));
	}
};

} } // namespace terark::rockeet

#endif // __terark_rockeet_index_Term_h__

#ifndef __terark_rockeet_index_Schema_h__
#define __terark_rockeet_index_Schema_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
# pragma warning(disable: 4127)
#endif

//#include <terark/util/refcount.hpp>
//#include <terark/io/MemStream.hpp>
//#include <terark/io/DataIO.hpp>
#include <terark/trb_cxx.hpp>

namespace terark { namespace rockeet {

class TERARK_DLL_EXPORT Field
{
    std::string  name;
    field_type_t type;
    int          offset;
    int          length;

public:
	static
    int
    trb_compare(
        const struct trb_vtab* vtab,
        const struct trb_tree* tree,
        const void* x,
        const void* y)
    {
        const Field* px = (const Field*)x;
        const Field* py = (const Field*)y;
        return px->name.compare(py->name);
    }

	Field(const std::string& name, field_type_t type, int offset)
		: name(name), type(type), offset(offset)
	{
		length = field_type_size(type);
	}

	const std::string& getName() const { return name; }

	int getType() const { return type; }
	int getOffset() const { return offset; }
	int getLength() const { return length; }

    void* getData(void* doc) const
    {
        return (char*)doc + offset;
    }
    const void* getData(const void* doc) const
    {
        return (const char*)doc + offset;
    }
    template<class T>
    T getCastData(const void* doc) const
    {
        return (T)((const char*)doc + offset);
    }
};

class TERARK_DLL_EXPORT Schema
{
    typedef trbtab<std::string, Field, 0, &Field::trb_compare> fieldset_t;
    fieldset_t fields;
    int lastOffset;
public:
	Schema()
	{
		lastOffset = 0;
	}
    void addField(const std::string& name, field_type_t type)
    {
        Field f(name, type, lastOffset);
        if (!fields.insert(f).second) {
            throw std::logic_error("duplicate field");
        }
		lastOffset += f.getLength();
    }
    const Field* getField(const std::string& name) const
    {
        fieldset_t::iterator iter = fields.find(name);
        if (fields.end() == iter)
            return NULL;
        return &*iter;
    }
	int fieldCount() const { return fields.size(); }
	int getDataSize() const { return lastOffset; }
};
/*
class TERARK_DLL_EXPORT VarSchema : public RefCounter
{
public:
	virtual ~VarSchema() {}
	virtual void load(const void* data, size_t length, void* pObj) const = 0;
	virtual void save(PortableDataOutput<AutoGrownMemIO>& output, void* pObj) const = 0;
};
typedef boost::intrusive_ptr<VarSchema> VarSchemaPtr;
*/

} } // namespace terark::rockeet

#endif // __terark_rockeet_index_Schema_h__

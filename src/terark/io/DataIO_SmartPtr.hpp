/* vim: set tabstop=4 : */
#pragma once

#include <utility>
#include <boost/smart_ptr.hpp>

namespace terark {

//!
#define DATA_IO_SMART_PTR_LOAD_SAVE(SmartPtrTemplate)			\
template<class T, class DataIO>									\
void DataIO_loadObject(DataIO& dio, SmartPtrTemplate<T>& x)		\
{																\
	x.reset(new T);												\
	dio >> *x;													\
}																\
template<class T, class DataIO>									\
void DataIO_saveObject(DataIO& dio, const SmartPtrTemplate<T>& x)\
{																\
	dio << *x;													\
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

DATA_IO_SMART_PTR_LOAD_SAVE(std::auto_ptr)
DATA_IO_SMART_PTR_LOAD_SAVE(boost::intrusive_ptr)
DATA_IO_SMART_PTR_LOAD_SAVE(boost::scoped_ptr)
DATA_IO_SMART_PTR_LOAD_SAVE(boost::shared_ptr)


} // namespace terark

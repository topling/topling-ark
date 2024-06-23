/* vim: set tabstop=4 : */
#ifndef __terark_io_load_save_file_h__
#define __terark_io_load_save_file_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#include "MemMapStream.hpp"
#include "DataIO.hpp"

//#include "../statistic_time.hpp"

namespace terark {

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// for convenient using...

#define TERARK_LOAD_FUNCTION_NAME    native_load_from_file
#define TERARK_SAVE_FUNCTION_NAME    native_save_to_file
#define TERARK_DATA_INPUT_CLASS      LittleEndianDataInput
#define TERARK_DATA_OUTPUT_CLASS     LittleEndianDataOutput
#define TERARK_DATA_INPUT_LOAD_FROM(input, x)  input >> x
#define TERARK_DATA_OUTPUT_SAVE_TO(output, x)  output << x
#include "load_save_iterator.h"


#define TERARK_LOAD_FUNCTION_NAME    portable_load_from_file
#define TERARK_SAVE_FUNCTION_NAME    portable_save_to_file
#define TERARK_DATA_INPUT_CLASS      PortableDataInput
#define TERARK_DATA_OUTPUT_CLASS     PortableDataOutput
#define TERARK_DATA_INPUT_LOAD_FROM(input, x)  input >> x
#define TERARK_DATA_OUTPUT_SAVE_TO(output, x)  output << x
#include "load_save_iterator.h"


#define TERARK_LOAD_FUNCTION_NAME    dump_load_from_file
#define TERARK_SAVE_FUNCTION_NAME    dump_save_to_file
#define TERARK_DATA_INPUT_CLASS      LittleEndianDataInput
#define TERARK_DATA_OUTPUT_CLASS     LittleEndianDataOutput
#define TERARK_DATA_INPUT_LOAD_FROM(input, x)  DataIO_dump_load_object(input, x)
#define TERARK_DATA_OUTPUT_SAVE_TO(output, x)  DataIO_dump_save_object(output, x)
#include "load_save_iterator.h"

/**
 @brief 更新文件
 */
#define TERARK_SAVE_FUNCTION_NAME    dump_update_file_only
#define TERARK_DATA_OUTPUT_CLASS     LittleEndianDataOutput
#define TERARK_DATA_OUTPUT_SAVE_TO(output, x)  x.dump_save(output)
#define TERARK_SAVE_FILE_OPEN_MODE "rb+" //!< 可读可写
#include "load_save_iterator.h"

template<class Object>
void dump_update_file(const Object& x, const std::string& szFile, bool printLog=true)
{
	try {
		dump_update_file_only(x, szFile, printLog);
	}
	catch (const OpenFileException&) {
		dump_save_to_file(x, szFile, printLog);
	}
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


} // namespace terark

#endif // __terark_io_load_save_file_h__

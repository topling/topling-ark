#ifndef __mapreduce_config_h__
#define __mapreduce_config_h__

#if defined(_MSC_VER)

# pragma once

#  if defined(MAP_REDUCE_CREATE_DLL)
#    pragma warning(disable: 4251)
#    define MAP_REDUCE_DLL_EXPORT __declspec(dllexport)      // creator of dll
#  elif defined(TERARK_USE_DLL)
#    pragma warning(disable: 4251)
#    define MAP_REDUCE_DLL_EXPORT __declspec(dllimport)      // user of dll
#    #if defined(_DEBUG) || !defined(NDEBUG)
#	   pragma comment(lib, "mapreduce-d.lib")
#    else
#	   pragma comment(lib, "mapreduce.lib")
#    endif
#  else
#    define MAP_REDUCE_DLL_EXPORT                            // static lib creator or user
#  endif

#else /* _MSC_VER */

#  define MAP_REDUCE_DLL_EXPORT

#endif /* _MSC_VER */

#include <terark/config.hpp>

#endif // __mapreduce_config_h__


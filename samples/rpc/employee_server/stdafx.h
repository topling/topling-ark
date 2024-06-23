#ifndef __stdafx_h__
#define __stdafx_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#include <stdio.h>
#include <terark/inet/SocketStream.hpp>

#ifdef EMPLOYEE_USE_BDB
#include <db_cxx.h>
#include <terark/bdb_util/dbmap.hpp>
#include <terark/bdb_util/kmapdset.hpp>
#else
#include <map>
#include <vector>
#endif

#endif // __stdafx_h__

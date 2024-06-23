CMAKE_MINIMUM_REQUIRED(VERSION 3.6)
PROJECT(terark-core)

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++17 -D_DARWIN_UNLIMITED_SELECT -D_GNU_SORUCE")
SET(CMAKE_CXX_STANDARD 14)

# Headers
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/src)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/zstd)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/boost-include)
INCLUDE_DIRECTORIES(/usr/local/include)

# CORE LIB
FILE(GLOB CORE_SRC src/terark/*.?pp
        src/terark/io/*.?pp
        src/terark/util/*.?pp
        src/terark/thread/*.?pp
        src/terark/succinct/*.?pp
        src/terark/c/*.c
        boost-include/libs/coroutine/src/*.?pp
        boost-include/libs/coroutine/src/detail/*.?pp
        boost-include/libs/coroutine/src/posix/*.?pp
        boost-include/libs/fiber/src/*.?pp
        boost-include/libs/fiber/src/algo/*.?pp
        boost-include/libs/fiber/src/numa/*.?pp
        boost-include/libs/fiber/examples/*.?pp
        boost-include/libs/fiber/examples/asio/*.?pp
        boost-include/libs/fiber/examples/cuda/*.?pp
        boost-include/libs/fiber/examples/hip/*.?pp
        boost-include/libs/fiber/examples/numa/*.?pp
        )
ADD_LIBRARY(terark-core SHARED ${CORE_SRC})


# FSA LIB
FILE(GLOB FSA_SRC src/terark/fsa/*.?pp)
ADD_LIBRARY(terark-fsa SHARED ${FSA_SRC})
TARGET_LINK_LIBRARIES(terark-fsa terark-core)


# ZSTD
FILE(GLOB ZSTD_SRC 3rdparty/zstd/zstd/common/*.c
        3rdparty/zstd/zstd/compress/*.c
        3rdparty/zstd/zstd/decompress/*.c
        3rdparty/zstd/zstd/deprecated/*.c
        3rdparty/zstd/zstd/dictBuilder/*.c
        3rdparty/zstd/zstd/legacy/*.c)

# ZBS LIB
FILE(GLOB ZBS_SRC src/terark/entropy/*.?pp
        src/terark/zbs/*.?pp
        ${ZSTD_SRC})
ADD_LIBRARY(terark-zbs SHARED ${ZBS_SRC})
TARGET_LINK_LIBRARIES(terark-zbs terark-core terark-fsa)


# RPC LIB
# FILE(GLOB RPC_SRC src/terark/inet/*.?pp src/terark/rpc/*.?pp)
# ADD_LIBRARY(terarkdb-rpc SHARED ${RPC_SRC})

# ZIP
# SET(ZIP_SRC src/terark/io/BzipStream.?pp src/terark/io/GzipStream.?pp)
# TRBXX
# SET(TRBXX_SRC src/terark/trb_cxx.?pp)
# C
# FILE(GLOB C_SRC src/terark/c/*.c src/terark/c/*.?pp)

############## Tests ##############


# CMake library under Linux won't accept multiple `main` functions, don't know how to fix it yet.
#IF (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    # tests
    FILE(GLOB TESTS_SRC
            test/bitfield_array/*.cpp
            test/fsa/*.cpp
            test/linebuf/*.cpp
            test/many/*.cpp
            test/need_args/*.cpp
            test/valvec/*.cpp
            tests/core/*.cpp
            tests/tries/*.cpp
            tests/succinct/*.cpp)
    ADD_LIBRARY(terark-tests SHARED ${TESTS_SRC})
    TARGET_LINK_LIBRARIES(terark-tests terark-zbs terark-fsa terark-core)

    # tools
    FILE(GLOB TOOLS_SRC
            tools/general/*.cpp
            tools/fsa/*.cpp
            tools/zbs/*.cpp)
    ADD_LIBRARY(terark-tools SHARED ${TOOLS_SRC})
    TARGET_LINK_LIBRARIES(terark-tools terark-zbs terark-fsa terark-core)
#ENDIF ()

#ifndef __mapreduce_HDFS_Stream_h__
#define __mapreduce_HDFS_Stream_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
# pragma warning(disable: 4127)
#endif

#include <terark/io/IStream.hpp>
//#include <mapreduce/src/config.hpp> // TODO: move to a better dir
#include "config.hpp" // TODO: move to a better dir

class MAP_REDUCE_DLL_EXPORT FileSystem
{
public:
    virtual ~FileSystem();

    virtual terark::IInputStream * openInput (const char* pathname, long long startOffset) = 0;
    virtual terark::IOutputStream* openOutput(const char* pathname, int flags) = 0;

	virtual size_t getBlockSize() = 0;

    /*
     * @return char*** blocksHosts[n blocks]
     *           ||
     *           \/
     *         block_0-->[host1, host2, ... NULL]
     *         block_1-->[host1, host2, ... NULL]
     *         ...
     *         block_n-->NULL
     *
     *  like vector<vector<string_host> >
     *
     *  @code
     *      char*** blocksHosts = fs->getHosts(path, start, length);
     *      for (int block = 0; NULL != blocksHosts[block]; ++block) {
     *          char** hosts = blocksHosts[block];
     *          for (int j = 0; hosts[j]; ++j) {
     *              const char* hostname = hosts[j];
     *              // using hostname
     *          }
     *      }
     *  @endcode
     */
    virtual char*** getHosts(const char* path, long long start, long long length) = 0;
    virtual void freeHosts(char*** blocksHosts) = 0;

	virtual const char* namenode() = 0;
};

typedef FileSystem* (*CreateFileSystemFP)(const char* host, int port, const char* user, const char* groups[], int ngroup);

MAP_REDUCE_DLL_EXPORT FileSystem* createLocalFileSystem();



#endif // __mapreduce_HDFS_Stream_h__



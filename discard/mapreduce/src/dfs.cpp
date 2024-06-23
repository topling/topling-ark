#include "dfs.h"
#include <terark/io/FileStream.hpp>

using namespace terark;

FileSystem::~FileSystem()
{
}

class LocalFileSystem : public FileSystem
{
public:
	virtual terark::IInputStream * openInput (const char* pathname, long long startOffset)
	{
		FileStream* fp = new FileStream(pathname, "rb");
		fp->seek(startOffset);
		return fp;
	}

	virtual terark::IOutputStream* openOutput(const char* pathname, int flags)
	{
		FileStream* fp = new FileStream(pathname, "wb");
		return fp;
	}

	virtual size_t getBlockSize()
	{
		return 4 * 1024;
	}

    virtual char*** getHosts(const char* path, long long start, long long length)
    {
		// bhs --- Block HostS
		static char localhost[] = "localhost";
		char*** bhs = (char***)malloc(sizeof(char**)*4);
		bhs[0] = (char**)&bhs[2];
		bhs[1] = NULL;
		bhs[0][0] = localhost;
		bhs[0][1] = NULL;
        return bhs;
    }

    virtual void freeHosts(char*** blocksHosts)
    {
		free(blocksHosts);
    }

	const char* namenode()
	{
		return "local";
	}
};

//FileSystem* createLocalFileSystem(const char* host, int port, const char* user, const char* groups[], int ngroup)
FileSystem* createLocalFileSystem()
{
	return new LocalFileSystem();
}



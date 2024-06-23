#ifndef __mapreduce_InputSplit_h__
#define __mapreduce_InputSplit_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
# pragma warning(disable: 4127)
#endif

#include "MapContext.h"

#include <terark/io/IStream.hpp>
#include <terark/io/StreamBuffer.hpp>
//#include <stdio.h>

class MAP_REDUCE_DLL_EXPORT ByteCountInputStream : public terark::IInputStream
{
	terark::IInputStream* input;
	int64_t nbytes;

public:
	explicit ByteCountInputStream(terark::IInputStream* input = 0) : input(input), nbytes(0) {}

	size_t read(void* buf, size_t length);
	bool eof() const;

	void attach(terark::IInputStream* input) { this->input = input; }
	terark::IInputStream* get() const { return input; }

	int64_t inputBytes() const { return nbytes; }
};

class FileSystem;
class MAP_REDUCE_DLL_EXPORT InputSplit
{
protected:
	FileSystem* fs;
	ByteCountInputStream input;
	terark::InputBuffer  ibuf; // make public
	int64_t startOffset;
	int64_t pos;      // pos in split
	int64_t length;   // estimated split length
	int64_t nr;       // NO. of input record
	bool    is_split; // split or not, if not split, one file is only one split

public:
	InputSplit(MapContext* context);
	virtual ~InputSplit();
};

class MAP_REDUCE_DLL_EXPORT Fixed_Record_Len_Input : public InputSplit
{
	uint32_t rlen; // record length = klen + vlen
	uint32_t klen; // key length
	uint32_t vlen; // vlen = rlen - klen
	size_t totalrecords;

public:
	Fixed_Record_Len_Input(MapContext* context);

	//! length of caller's recbuf must >= rlen
	bool read(void* recbuf);
};

class MAP_REDUCE_DLL_EXPORT TextInput : public InputSplit
{
public:
	TextInput(MapContext* context);

	void getline(std::string& line);
};

#endif //__mapreduce_InputSplit_h__


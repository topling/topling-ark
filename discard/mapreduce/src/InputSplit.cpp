#include "InputSplit.h"

#include "dfs.h"

#include <stdexcept>
#include <terark/stdtypes.hpp>
#include <terark/io/IOException.hpp>

using namespace terark;

size_t ByteCountInputStream::read(void* buf, size_t length)
{
	assert(NULL != input);
	size_t n = input->read(buf, length);
	this->nbytes = n;
	return n;
}

bool ByteCountInputStream::eof() const
{
	assert(NULL != input);
	return input->eof();
}

InputSplit::InputSplit(MapContext* context)
	: fs(NULL),
	  ibuf(&input)
{
	startOffset = context->getll("MapInput.split.startOffset", -1);
	length      = context->getll("MapInput.split.length", -1);
	if (-1 == length) {
		startOffset = 0;
		length = LLONG_MAX;
		is_split = false;
	}
	else if (-1 == startOffset) {
		fprintf(stderr, "invalid: startOffset=-1\n");
		exit(1);
	}
	else {
		is_split = true;
	}
	pos = 0;

	const char* mapInputFile = "MapInput.file.path";
	const std::string file = context->get(mapInputFile, "");
	if (file.empty()) {
		fprintf(stderr, "'%s' is an empty string or not defined\n", mapInputFile);
		exit(1);
	}

	fs = context->getFileSystem();

	IInputStream* is = fs->openInput(file.c_str(), startOffset);
	if (NULL == is) {
		fprintf(stderr, "openInput('%s') failed, namenode.host=%s\n", file.c_str(), fs->namenode());
		delete fs;
		fs = NULL;
		exit(1);
	}
	input.attach(is);
}

InputSplit::~InputSplit()
{
	if (NULL != input.get())
		delete input.get();

	if (NULL != fs)
		delete fs;
}

Fixed_Record_Len_Input::Fixed_Record_Len_Input(MapContext* context)
	: InputSplit(context)
{
	rlen = context->getll("MapInput.record.length", 0);
	klen = context->getll("MapInput.key.length", 0);
	vlen = context->getll("MapInput.val.length", 0);

	if (0 == rlen) {
		if (0 == klen || 0 == vlen) {
		//	throw std::invalid_argument("Fixed_Record_Len_Input: 0 == rlen AND (0 == klen OR 0 == vlen)");
			fprintf(stderr, "Fixed_Record_Len_Input: 0 == rlen AND (0 == klen OR 0 == vlen)\n");
			exit(1);
		}
		rlen = klen + vlen;
	}
	else {
		if ((0 != klen || 0 != vlen) && klen + vlen != rlen) {
		//	throw std::invalid_argument("Fixed_Record_Len_Input: (0 != klen || 0 != vlen) && klen + vlen != rlen");
			fprintf(stderr, "Fixed_Record_Len_Input: (0 != klen || 0 != vlen) && klen + vlen != rlen\n");
			exit(1);
		}
	}
	startOffset = align_up(startOffset, rlen);
	length      = align_up(length, rlen);
	totalrecords= length / rlen;
}

//! length of caller's recbuf must >= rlen
bool Fixed_Record_Len_Input::read(void* recbuf)
{
	if (nr < totalrecords) {
		ibuf.ensureRead(recbuf, rlen);
		++nr;
		return true;
	}
	else {
		return false;
	}
}

TextInput::TextInput(MapContext* context)
	: InputSplit(context)
{
	if (is_split && startOffset > 0) {
		std::string  discardFirstLine;
		ibuf.getline(discardFirstLine, INT_MAX);
	}
}

void TextInput::getline(std::string& line)
{
	if (is_split) {
		if (pos <= length) {
			ibuf.getline(line, INT_MAX);
			++nr;
		}
		else {
			throw terark::EndOfFileException("TextInput::getline");
		}
	}
	else {
		ibuf.getline(line, INT_MAX);
		++nr;
	}
}


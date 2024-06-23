/* vim: set tabstop=4 : */
// for convenient using...
//////////////////////////////////////////////////////////////////////////

// optional param:
//   TERARK_DATA_IO_LS_EX_T_PARAM
//   TERARK_DATA_IO_LS_EX_PARAM

#ifndef TERARK_DATA_IO_LS_EX_T_PARAM
#define TERARK_DATA_IO_LS_EX_T_PARAM
#endif

#ifndef TERARK_DATA_IO_LS_EX_PARAM
#define TERARK_DATA_IO_LS_EX_PARAM
#endif

#ifdef TERARK_LOAD_FUNCTION_NAME

// parameter for this file
//   TERARK_LOAD_FUNCTION_NAME
//   TERARK_DATA_INPUT_CLASS
//   TERARK_DATA_INPUT_LOAD_FROM(input, x)

template<class Object TERARK_DATA_IO_LS_EX_T_PARAM>
bool TERARK_LOAD_FUNCTION_NAME(Object& x TERARK_DATA_IO_LS_EX_PARAM,
							   const std::string& szFile,
							   bool fileMustExist=true,
							   bool printLog=true)
{
// 	std::auto_ptr<StatisticTime> pst;
// 	if (printLog)
// 	{
// 		string_appender<> oss;
// 		oss << BOOST_STRINGIZE(TERARK_LOAD_FUNCTION_NAME) << ": \"" << szFile << "\"";
// 		pst.reset(new StatisticTime(oss.str(), std::cout));
// 	}
	FileStream file;
	file.open(szFile.c_str(), "rb");
	if (file.isOpen()) {
#ifdef TERARK_LOAD_SAVE_CONVENIENT_USE_STREAM_BUFFER
		setvbuf(file, 0, _IONBF, 0);
		SeekableStreamWrapper<FileStream*> fileWrapper(&file);
		SeekableInputBuffer sbuf(16*1024);
		sbuf.attach(&fileWrapper);
		TERARK_DATA_INPUT_CLASS<SeekableInputBuffer> input(&sbuf);
#else
		TERARK_DATA_INPUT_CLASS<FileStream*> input(&file);
#endif
		TERARK_DATA_INPUT_LOAD_FROM(input, x);
	}
	else
	{
		if (fileMustExist)
			FileStream::ThrowOpenFileException(szFile.c_str(), "rb");
		else {
			OpenFileException exp(szFile.c_str(), "mode=rb");
			if (printLog)
				printf("open file failed: %s\n", exp.what());
			return false;
		}
	}
	return true;
}

template<class Object TERARK_DATA_IO_LS_EX_T_PARAM>
void TERARK_LOAD_FUNCTION_NAME(Object& x TERARK_DATA_IO_LS_EX_PARAM,
							   FileStream& file,
							   const std::string& szTitle,
							   bool printLog=true)
{
// 	std::auto_ptr<StatisticTime> pst;
// 	if (printLog)
// 	{
// 		string_appender<> oss;
// 		oss << BOOST_STRINGIZE(TERARK_LOAD_FUNCTION_NAME) << ", title: \"" << szTitle << "\"";
// 		pst.reset(new StatisticTime(oss.str(), std::cout));
// 	}
#ifdef TERARK_LOAD_SAVE_CONVENIENT_USE_STREAM_BUFFER
//	setvbuf(file, 0, _IONBF, 0);
	SeekableStreamWrapper<FileStream*> fileWrapper(&file);
	SeekableInputBuffer sbuf(16*1024);
	sbuf.attach(&fileWrapper);
	TERARK_DATA_INPUT_CLASS<SeekableInputBuffer> input(&sbuf);
#else
	TERARK_DATA_INPUT_CLASS<FileStream*> input(&file);
#endif
	TERARK_DATA_INPUT_LOAD_FROM(input, x);
}

template<class Object>
bool TERARK_LOAD_FUNCTION_NAME(pass_by_value<Object> x,
							   const std::string& szFile,
							   bool fileMustExist=true,
							   bool printLog=true)
{
	return TERARK_LOAD_FUNCTION_NAME(x.get(), szFile, fileMustExist, printLog);
}

template<class Object>
void TERARK_LOAD_FUNCTION_NAME(pass_by_value<Object> x,
							   FileStream& file,
							   const std::string& szTitle,
							   bool printLog=true)
{
	TERARK_LOAD_FUNCTION_NAME(x.get(), file, szTitle, printLog);
}
#endif // TERARK_LOAD_FUNCTION_NAME

//////////////////////////////////////////////////////////////////////////

#ifdef TERARK_SAVE_FUNCTION_NAME

// parameter for this file
//   TERARK_SAVE_FUNCTION_NAME
//   TERARK_DATA_OUTPUT_CLASS
//   TERARK_DATA_OUTPUT_SAVE_TO(output, x)
template<class Object TERARK_DATA_IO_LS_EX_T_PARAM>
void TERARK_SAVE_FUNCTION_NAME(const Object& x TERARK_DATA_IO_LS_EX_PARAM,
							   const std::string& szFile,
							   bool printLog=true)
{
// 	std::auto_ptr<StatisticTime> pst;
// 	if (printLog)
// 	{
// 		string_appender<> oss;
// 		oss << BOOST_STRINGIZE(TERARK_SAVE_FUNCTION_NAME) << ": \"" << szFile << "\"";
// 		pst.reset(new StatisticTime(oss.str(), std::cout));
// 	}
#ifdef TERARK_SAVE_FILE_OPEN_MODE
	const char* openMode = TERARK_SAVE_FILE_OPEN_MODE;
#undef TERARK_SAVE_FILE_OPEN_MODE
#else
	const char* openMode = "wb";
#endif
	FileStream file(szFile.c_str(), openMode);

#ifdef TERARK_LOAD_SAVE_CONVENIENT_USE_STREAM_BUFFER
	setvbuf(file, 0, _IONBF, 0);
	SeekableStreamWrapper<FileStream*> fileWrapper(&file);
	SeekableOutputBuffer sbuf(16*1024);
	sbuf.attach(&fileWrapper);
	TERARK_DATA_OUTPUT_CLASS<SeekableOutputBuffer> output(&sbuf);
#else
	TERARK_DATA_OUTPUT_CLASS<FileStream*> output(&file);
#endif
	TERARK_DATA_OUTPUT_SAVE_TO(output, x);
}

template<class Object TERARK_DATA_IO_LS_EX_T_PARAM>
void TERARK_SAVE_FUNCTION_NAME(const Object& x TERARK_DATA_IO_LS_EX_PARAM,
							   FileStream& file,
							   const std::string& szTitle,
							   bool printLog=true)
{
// 	std::auto_ptr<StatisticTime> pst;
// 	if (printLog)
// 	{
// 		string_appender<> oss;
// 		oss << BOOST_STRINGIZE(TERARK_SAVE_FUNCTION_NAME) << ", title: \"" << szTitle << "\"";
// 		pst.reset(new StatisticTime(oss.str(), std::cout));
// 	}
#ifdef TERARK_LOAD_SAVE_CONVENIENT_USE_STREAM_BUFFER
//	setvbuf(file, 0, _IONBF, 0);
	SeekableStreamWrapper<FileStream*> fileWrapper(&file);
	SeekableOutputBuffer sbuf(16*1024);
	sbuf.attach(&fileWrapper);
	TERARK_DATA_OUTPUT_CLASS<SeekableOutputBuffer*> output(&sbuf);
#else
	TERARK_DATA_OUTPUT_CLASS<FileStream*> output(&file);
#endif
	TERARK_DATA_OUTPUT_SAVE_TO(output, x);
}

#endif // TERARK_SAVE_FUNCTION_NAME

#undef TERARK_DATA_IO_LS_EX_T_PARAM
#undef TERARK_DATA_IO_LS_EX_PARAM

#undef TERARK_LOAD_FUNCTION_NAME
#undef TERARK_SAVE_FUNCTION_NAME

#undef TERARK_DATA_INPUT_CLASS
#undef TERARK_DATA_INPUT_LOAD_FROM

#undef TERARK_DATA_OUTPUT_CLASS
#undef TERARK_DATA_OUTPUT_SAVE_TO

#include <Reducer.h>
#include <terark/trb_cxx.hpp>
#include <terark/io/IStream.hpp>
#include <terark/io/StreamBuffer.hpp>
#include <terark/io/DataIO.hpp>

using namespace terark;

class WcReduce : public Reducer
{
	trbstrmap<int> wcmap;
	IOutputStream* textOutput;
	IOutputStream*  binOutput;

public:
	WcReduce(ReduceContext* context)
		: Reducer(context)
	{
		textOutput = createOutput("text");
		 binOutput = createOutput("bin" );
	}

	~WcReduce()
	{
	}

	// @override
	// will be called by the frame when read every key-value from mappers
	void reduce(const void* key, size_t klen, const void* val, size_t vlen)
	{
		assert(sizeof(int) == vlen);
		int cnt;
		memcpy(&cnt, val, sizeof(int));
		const char* word = (const char*)key;
		wcmap[word] += cnt;
	}

	// this is a multi-output example
	// write to text output
	void writeText()
	{
		FILE* fp = textOutput->forOutputFILE();
		for (trbstrmap<int>::const_iterator i = wcmap.begin(); i != wcmap.end(); ++i)
		{
			int cnt = *i;
			fprintf(fp, "%.*s\t%d\n", (int)wcmap.klen(i), wcmap.key(i), cnt);
		}
		fclose(fp);
	}

	// this is a multi-output example
	// write to bin output
	void writeBin()
	{
		PortableDataOutput<OutputBuffer> obuf; obuf.attach(textOutput);
		for (trbstrmap<int>::const_iterator i = wcmap.begin(); i != wcmap.end(); ++i)
		{
			var_uint32_t cnt(*i);
			obuf << var_uint32_t(wcmap.klen(i));
			obuf.ensureWrite(wcmap.key(i), wcmap.klen(i));
			obuf << cnt;
		}
	}
};

int main(int argc, char* argv[])
{
	ReduceContext context(argc, argv);
	WcReduce reduce(&context);
	reduce.run(&context);
	reduce.writeText();
	reduce.writeBin();
	return 0;
}


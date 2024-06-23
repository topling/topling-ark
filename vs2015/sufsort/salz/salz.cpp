// salz.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <terark/valvec.hpp>
#include <terark/fsa/nest_louds_trie.hpp>
#include <terark/util/linebuf.hpp>
#include <terark/util/profiling.hpp>
#include <terark/fsa/suffix_array_trie.hpp>
#include <terark/io/FileStream.hpp>
#include <terark/io/StreamBuffer.hpp>
#include <terark/io/DataIO.hpp>

using namespace terark;

int main(int argc, char* argv[]) {
	SortableStrVec  strVec;
	profiling pf;
	LineBuf line;
  {
	Auto_fclose fp;
	if (1) {
	#ifdef _MSC_VER
		if (_setmode(_fileno(stdin), _O_BINARY) < 0) {
			THROW_STD(invalid_argument, "set stdout as binary mode failed");
		}
	#endif
		while (!feof(fp.self_or(stdin))) {
			int32_t reclen;
			if (fread(&reclen, 1, 4, fp.self_or(stdin)) != 4) {
				break;
			}
			if (reclen <= 8) {
				fprintf(stderr, "read binary data error: nth=%ld reclen=%ld too small\n"
						, long(strVec.size()), long(reclen));
				break;
			}
			strVec.push_back("");
			strVec.back_grow_no_init(reclen-4);
			long datalen = fread(strVec.mutable_nth_data(strVec.size()-1), 1, reclen-4, fp.self_or(stdin));
			if (datalen != reclen-4) {
				fprintf(stderr, "read binary data error: nth=%ld requested=%ld returned=%ld\n"
						, long(strVec.size()), long(reclen-4), datalen);
				strVec.pop_back();
				break;
			}
		}
	} else {
		while (line.getline(fp.self_or(stdin)) > 0) {
			line.chomp();
			strVec.push_back(line);
		}
	}
  }
	const char* fpath = "lztrie.tmp";
	{
		SuffixTrieCacheDFA suf;
		suf.build_sa(strVec);
		suf.bfs_build_cache(8, 32, 8);
		suf.make_lz_trie(fpath, 30);
	}
	FileStream fp;
	NativeDataInput<InputBuffer> dio;
	dio.attach(&fp);
	fp.open(fpath, "rb");
	fp.disbuf();
	SortableStrVec strVec2;
	strVec2.m_index.reserve(strVec.size() * 2);
	strVec2.m_strpool.reserve(strVec.str_size());
	for (size_t i = 0; i < strVec.size(); ++i) {
		fstring str = strVec[i];
		for (size_t j = 0; j < str.size(); ) {
			uint32_t pos = dio.load_as<uint32_t>();
			uint32_t len = dio.load_as<uint32_t>();
			strVec2.push_back(fstring(strVec.m_strpool.data() + pos, len));
			j += len;
		}
	}
	NestLoudsTrieBlobStore_SE_512 store;
	NestLoudsTrieConfig conf;
	conf.initFromEnv();
	conf.nestLevel = 3;
	store.build_from(strVec2, conf);
	store.save_mmap("lztrie.nlt");
    return 0;
}


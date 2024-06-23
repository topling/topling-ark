#include <terark/zbs/ZstdStream.hpp>
#include <terark/io/IOException.hpp>
#include <terark/io/IStream.hpp>
#include <terark/stdtypes.hpp>
#include <terark/util/refcount.hpp>
#include <memory>
#include <iostream>
#include <fstream>
#include <chrono>
#include <thread>

using namespace terark;

class TestInputStream: public IInputStream {
public:
    TestInputStream(const char* fname) {
        f = fopen(fname, "rb");
    }
    size_t read(void* vbuf, size_t length) {
        return fread(vbuf, 1, length, f);
    }
    bool eof() const {
        return feof(f);
    }

    FILE* f;
};

class TestOutputStream: public IOutputStream {
public:
    TestOutputStream(const char* fname) {
        f = fopen(fname, "wb");
    }
    size_t write(const void* vbuf, size_t length) {
        return fwrite(vbuf, 1, length, f);
    }
    void flush() {
        fflush(f);
    }

    FILE* f;
};



int main(int argc, char* argv[])
{
    const char* wiki_file = "/node-shared/wikipedia.flat.no.tab.shuf";
    const char* compress_file = "/home/gukaifeng/zstd_test/wiki_compress_file";

    size_t wiki_file_size = 0;
    size_t buf_size = 2000000000;
    void* buf = malloc(buf_size);

    {
        // 先把 wiki_file 全都读到一个足够大的缓冲区 buf 里，用来模拟输入流
        FILE* f = fopen(wiki_file, "rb");
        if (f == nullptr) {
            std::cout << "f is nullptr" << std::endl;
            exit(1);
        }
        while (!feof(f)) {
            wiki_file_size += fread((char*)buf + wiki_file_size, 1, 1000, f);
        }
        printf("已读取 %s 完成，待压缩流中的字节数为 %zu。\n", wiki_file, wiki_file_size);
        fclose(f);
    }


    {
        // 压缩
        TestOutputStream tos(compress_file);
        std::unique_ptr<ZstdOutputStream> zos( new ZstdOutputStream(&tos));
        size_t compress_per_size = wiki_file_size;  // 这里先一次都压缩了，为了计时
        size_t compressed_size = 0;
        std::chrono::high_resolution_clock::time_point tp1 = std::chrono::high_resolution_clock::now();  // 计时开始
        while (compressed_size < wiki_file_size) {
            compressed_size += zos->write((char*)buf + compressed_size, compress_per_size);
        }
        std::chrono::high_resolution_clock::time_point tp2 = std::chrono::high_resolution_clock::now();  // 计时结束
        std::chrono::duration<size_t, std::nano> dur = tp2 - tp1;  // 计算耗时
        zos->close();
        tos.flush();
        printf("已压缩完成 %zu 字节，", compressed_size);
        std::cout << "耗时 " << std::chrono::duration_cast<std::chrono::milliseconds>(dur).count() << " 毫秒。"
                  << "压缩好的文件存储在 " << compress_file;
        // 析构或者 close() 才能结束压缩
    }

    {
        // 因为压缩程序中没统计压缩完成后的大小，所以在这里读一下文件，算一下大小
        FILE* read_f = fopen(compress_file, "rb");
        void* read_buf = malloc(buf_size);
        size_t compressed_file_size = 0;
        while (!feof(read_f)) {
            compressed_file_size += fread(read_buf, 1, 10000, read_f);
        }
        printf("。压缩完成后的大小为 %zu 字节，压缩率为 %lf %%。\n", compressed_file_size, (double)compressed_file_size / wiki_file_size * 100);
        free(read_buf);
        fclose(read_f);
    }

    {
        // 解压
        TestInputStream tis(compress_file);
        ZstdInputStream* zis = new ZstdInputStream(&tis);
        void* decompress_buf = malloc(buf_size);
        size_t decompress_per_size = buf_size;
        size_t decompressed_size = 0;
        std::chrono::high_resolution_clock::time_point tp1 = std::chrono::high_resolution_clock::now();  // 计时开始
        while (!zis->eof()) {
            decompressed_size += zis->read((char*)decompress_buf + decompressed_size, decompress_per_size);
        }
        std::chrono::high_resolution_clock::time_point tp2 = std::chrono::high_resolution_clock::now();  // 计时结束
        std::chrono::duration<size_t, std::nano> dur = tp2 - tp1;  // 计算耗时
        printf("已解压完成，解压后大小为 %zu 字节。 ", decompressed_size);
        std::cout << "在缓冲区足够大时，解压缩耗时 " << std::chrono::duration_cast<std::chrono::milliseconds>(dur).count() << " 毫秒。 \n";
        delete zis;

        {
            // 对比压缩前后的一部分内容，验证正确性
            size_t compare_s = 45678;
            size_t compare_e = 45789;
            printf("\n压缩前，wiki_file 的第 %zu —— %zu 个字节为：", compare_s, compare_e);
            for (size_t i = compare_s; i < compare_e; ++i) {
                printf("%c", ((char*)buf)[i]);
            }
            printf("\n");

            printf("解压后，wiki_file 的第 %zu —— %zu 个字节为：", compare_s, compare_e);
            for (size_t i = compare_s; i < compare_e; ++i) {
                printf("%c", ((char*)decompress_buf)[i]);
            }
            printf("\n");
        }

        free(decompress_buf);
    }



    free(buf);


    return 0;
}
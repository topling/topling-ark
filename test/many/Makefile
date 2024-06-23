CXXFLAGS = -g3

SRCS += $(wildcard *.cpp)

%/pinyin2word.o : override CXXFLAGS += -DTRANSFORMER_DECODER_MAIN
%/synonym_dict.o : override CXXFLAGS += -DSYNONYM_BUILDER
%/rank_select_benchmark.o : override CXXFLAGS += -I../../../sdsl-lite/include

TERARK_EXT_LIBS := zbs fsa

include ../../tools/fsa/Makefile.common


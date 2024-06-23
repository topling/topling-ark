
EXE_SRCS += $(wildcard src/aho_corasick/*.cpp)
#EXE_SRCS += $(wildcard src/aho_corasick/re2/*.cpp)
EXE_SRCS += $(wildcard src/dawg/*.cpp)
EXE_SRCS += $(wildcard src/pinyin/*.cpp)
EXE_SRCS += $(wildcard src/adfa/*.cpp)
EXE_SRCS += $(wildcard src/spell/*.cpp)
EXE_SRCS += $(wildcard src/nfa/*.cpp)
EXE_SRCS += $(wildcard src/suffix_dfa/*.cpp)
EXE_SRCS += $(wildcard src/benchmark/*.cpp)

%/pinyin2word.o : override CXXFLAGS += -DTRANSFORMER_DECODER_MAIN
%/synonym_dict.o : override CXXFLAGS += -DSYNONYM_BUILDER

TERARK_EXT_LIBS := fsa

include ../../tools/fsa/Makefile.common


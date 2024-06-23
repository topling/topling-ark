
TERARK_HOME = ../..

CXXFLAGS := -g3
CXXFLAGS += -Wall -Wextra
CXXFLAGS += -I${TERARK_HOME}/src
CXXFLAGS += -I${TERARK_HOME}/3rdparty/re2
CXXFLAGS += -I/home/leipeng/opt/include
#LDFLAGS = -g3

CXX = g++
#CXX = env LD_LIBRARY_PATH=${HOME}/opt/lib:${HOME}/opt/lib64 ${HOME}/opt/bin/g++
#-std=gnu++11

.PHONY: all clean

SRC := $(wildcard *.cpp)
BIN_R := $(addprefix rls/, $(basename ${SRC}))
BIN_D := $(addprefix dbg/, $(basename ${SRC}))

ifeq (${STATIC},1)
  LIB_D := -Wl,--whole-archive ${TERARK_HOME}/lib/libterark-fsa_all-d.a -Wl,--no-whole-archive -lz -lbz2 -lrt -pthread
  LIB_R := -Wl,--whole-archive ${TERARK_HOME}/lib/libterark-fsa_all-r.a -Wl,--no-whole-archive -lz -lbz2 -lrt -pthread
else
  LIB_D := -L${TERARK_HOME}/lib -lterark-fsa-d -lterark-core-d
  LIB_R := -L${TERARK_HOME}/lib -lterark-fsa-r -lterark-core-r
endif

all: ${BIN_R} ${BIN_D}

rls/% : rls/%.o
	@#echo Release linking ....
	${CXX} -o $@ $^ ${LDFLAGS} ${LIB_R}

dbg/% : dbg/%.o
	@#echo Debug linking ....
	${CXX} -o $@ $^ ${LDFLAGS} ${LIB_D}

rls/%.o : %.cpp
	@#echo Release compiling ...
	@mkdir -p rls
	${CXX} -c -O3 ${CXXFLAGS} -DNDEBUG -o $@ $^

dbg/%.o : %.cpp
	@#echo Debug compiling ...
	@mkdir -p dbg
	${CXX} -c -O0 ${CXXFLAGS} -o $@ $^

clean:
	rm -rf dbg rls


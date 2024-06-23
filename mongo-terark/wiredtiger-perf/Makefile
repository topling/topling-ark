
CXXFLAGS += -O3
CXXFLAGS += -I../../src
LDFLAGS += -lwiredtiger

.PHONY: all

all: benchmark

benchmark: benchmark.o

% : %.o
	${CXX} -o $@ $< ${LDFLAGS} 

%.o : %.cpp
	${CXX} ${CXXFLAGS} -c -o $@ $<


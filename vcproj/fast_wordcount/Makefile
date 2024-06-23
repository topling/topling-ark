
TERARK_INC = -I../../src
CXXFLAGS += -g3
CXXFLAGS += -O0
#CXXFLAGS += -DNDEBUG

wordcount : wordcount.cpp
	${CXX} ${CXXFLAGS} -Wall -Wextra ${TERARK_INC} -o $@ $^

wordcount-r : wordcount.cpp
	${CXX} -O3 -g3 -Wall -Wextra ${TERARK_INC} -o $@ $^



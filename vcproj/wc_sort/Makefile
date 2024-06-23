
TERARK_INC = -I../../src
CXXFLAGS += -Wconversion
#CXXFLAGS += -Wno-sign-conversion
CXXFLAGS += -g3
CXXFLAGS += -O0
#CXXFLAGS += -DNDEBUG

wc_sort : wc_sort.cpp
	${CXX} ${CXXFLAGS} -Wall -Wextra ${TERARK_INC} -o $@ $^

wc_sort-r : wc_sort.cpp
	${CXX} -O3 -g3 -Wall -Wextra ${TERARK_INC} -o $@ $^

clean :
	rm -f wc_sort wc_sort-r

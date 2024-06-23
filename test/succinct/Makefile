
BASE=$(shell cd ../../..;pwd)
TerarkDir=$(BASE)/terark/src
TerarkLib=$(BASE)/topling-rocks/pkg/topling-rocks-Linux-x86_64-g++-4.8-bmi2-0
CXX=g++
CFLAGS=-g3 -std=gnu++17 -fPIC
INC=-I. -I.. -I$(TerarkDir)
LIBPATH=-L. -L.. -L$(TerarkLib)/lib
LIB=-lpthread -lterark-core-g++-4.8-d -lterark-fsa-g++-4.8-d -lterark-zbs-g++-4.8-d
OBJ=rank_select_fewzero.o

all: test_rs_fewzero

test_rs_fewzero: ${OBJS}
	${CXX} -g3 -std=gnu++17 ${INC} ${LIBPATH} -o rs_fewzero_ut rs_fewzero_ut.cc ${OBJS} ${LIB}

rank_select_fewzero.o:
	${CXX} -g3 -std=gnu++17 ${INC} ${LIBPATH} -c rank_select_fewzero.cpp
clean:
	rm -f ${EXES} *.o *.exe *.a test_add bridge *.so* *.gcno *.gcda *.gcov rs_fewzero_ut



TERARK_HOME?=$(shell cd ../../..; pwd)
TERARK_INC ?=${TERARK_HOME}/src

include ${TERARK_HOME}/Make.config.mk

INCS += -I/usr/src/hadoop-0.20/c++/libhdfs
INCS += -I/usr/lib/jvm/java-6-sun-1.6.0.15/include
INCS += -I/usr/lib/jvm/java-6-sun-1.6.0.15/include/linux

# for #include <mapreduce/src/config.h>
INCS += -I${TERARK_HOME}/

SRCS    := $(wildcard *.cpp)

CXXFLAGS += -Wall
CXXFLAGS += -Wno-invalid-offsetof

#LIBS += -L/D/jlib/hadoop-0.20.1+152/c++/Linux-amd64-64/lib
LIBS += -L/D/jlib/hadoop-0.19.1/libhdfs/
LIBS += -lhdfs

ifeq (YES, ${DEBUG})
   LIBS += -lterark-fsa_all-${COMPILER}-d
   OUTPUT = libmapreduce-hdfs-${COMPILER}-d.so
else
   LIBS += -lterark-${COMPILER}
   OUTPUT = libmapreduce-hdfs-${COMPILER}.so
endif

#LIBS += -lz -lbz2

include ${TERARK_HOME}/Make.rules.mk



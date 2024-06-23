include ../../../Make.config.mk

SRCS 	:= $(wildcard *.cpp)
TERARK_INC:=$(shell cd ../../../src; pwd)

ifeq (YES, ${DEBUG})
   LIBS += -lterark-fsa_all-${COMPILER}-d
   LIBS += -lterark-bdb_util-${COMPILER}-d
   OUTPUT = d
else
   LIBS += -lterark-${COMPILER}
   LIBS += -lterark-bdb_util-${COMPILER}
   OUTPUT = a
endif

#LIBS += -lboost_serialization${BOOST_SUFFIX}
LIBS += -lz -lbz2
BDB_VER ?= 4.8
BDB_HOME ?= /usr/local/BerkeleyDB.4.8
INCS += -I${BDB_HOME}/include
LIBS += -L${BDB_HOME}/lib
#LIBS += -ldb-${BDB_VER} -ldb_cxx-${BDB_VER}
LIBS += -ldb -ldb_cxx

include ../../../Make.rules.mk


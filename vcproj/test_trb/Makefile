include ../../Make.config.mk

TERARK_INC=../../src
SRCS    := $(wildcard *.cpp)

ifeq (YES, ${DEBUG})
   LIBS += -lterark-fsa_all-${COMPILER}-d
   OUTPUT = d
else
   LIBS += -lterark-${COMPILER}
   OUTPUT = a
endif

#LIBS += -lboost_serialization${BOOST_SUFFIX}
#LIBS += -ldb-4.7 -ldb_cxx-4.7
LIBS += -lz -lbz2
CXXFLAGS += -Wno-invalid-offsetof

include ../../Make.rules.mk


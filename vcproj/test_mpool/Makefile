TERARK_HOME?=$(shell cd ../..; pwd)
TERARK_INC ?=${TERARK_HOME}/src

include ${TERARK_HOME}/Make.config.mk

SRCS    := stdafx.cpp test_mpool.cpp

CXXFLAGS += -Wall
CXXFLAGS += -Wno-invalid-offsetof

ifeq (YES, ${DEBUG})
   LIBS += -lterark-fsa_all-${COMPILER}-d
   OUTPUT = d
else
   LIBS += -lterark-${COMPILER}
   OUTPUT = a
endif

LIBS += -lboost_regex${BOOST_SUFFIX}
LIBS += -lz -lbz2

include ${TERARK_HOME}/Make.rules.mk



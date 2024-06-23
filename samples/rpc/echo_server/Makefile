include ../../../Make.config.mk

SRCS 	:= $(wildcard *.cpp)
TERARK_INC:=$(shell cd ../../../src; pwd)

ifeq (YES, ${DEBUG})
   LIBS += -lterark-fsa_all-${COMPILER}-d
   OUTPUT = d
else
   LIBS += -lterark-${COMPILER}
   OUTPUT = a
endif

#LIBS += -lboost_serialization${BOOST_SUFFIX}
#LIBS += -lz -lbz2

include ../../../Make.rules.mk


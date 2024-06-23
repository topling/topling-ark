include ../../../Make.config.mk

SRCS    := $(wildcard *.cpp)
TERARK_INC = ../../../src

ifeq (YES, ${DEBUG})
   LIBS += -lterark-fsa_all-${COMPILER}-d
   OUTPUT = d
else
   LIBS += -lterark-${COMPILER}
   OUTPUT = a
endif

LIBS += -lz -lbz2
#LIBS += -boost_serialization
CXXFLAGS += -Wno-invalid-offsetof

include ../../../Make.rules.mk


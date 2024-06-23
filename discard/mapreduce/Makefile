TERARK_HOME?=$(shell cd ..; pwd)
TERARK_INC ?=${TERARK_HOME}/src

include ${TERARK_HOME}/Make.config.mk

INCS	+= -I${TERARK_HOME}/mapreduce/dfs
INCS	+= -I${TERARK_HOME}
SRCS    := $(wildcard src/*.cpp)

CXXFLAGS += -Wall
CXXFLAGS += -Wno-invalid-offsetof

ifeq (YES, ${DEBUG})
   LIBS += -lterark-fsa_all-${COMPILER}-d
   OUTPUT = libmapreduce-${COMPILER}-d.so
else
   LIBS += -lterark-${COMPILER}
   OUTPUT = libmapreduce-${COMPILER}.so
endif

#LIBS += -lz -lbz2

include ${TERARK_HOME}/Make.rules.mk



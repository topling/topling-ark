include ../../Make.config.mk

SRCS    := $(wildcard *.cpp)

ifeq (YES, ${DEBUG})
   LIBS += -lterark-fsa_all-${COMPILER}-d
   OUTPUT = d
else
   LIBS += -lterark-${COMPILER}
   OUTPUT = a
endif

LIBS += -lz -lbz2
CXXFLAGS += -Wno-invalid-offsetof

include ../../Make.rules.mk



COMPILER := $(shell ${CXX} --version | head -1 | sed 's/\(\S\+\)\s\+([^()]*)\s\+\([0-9]\+.[0-9]\+\).*/\1-\2/')

#override CXXFLAGS += -I ../../../src

CompileSpeedTerark : CompileSpeedTerark.cpp
	    ${CXX} ${CXXFLAGS} -time -I../../../src -L../../../lib -lterark-fsa_all-${COMPILER}-d -lboost_system${BOOST_SUFFIX} -o $@ $^


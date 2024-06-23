
CXX := $(shell if test -f /opt/bin/g++; then echo /opt/bin/g++; elif test -f ${HOME}/opt/bin/g++; then echo ${HOME}/opt/bin/g++; else echo g++; fi)

CXXFLAGS = -Wall -Wextra -Wno-parentheses -std=c++1y -I../../src -I/opt/include -I$(BOOST_DIR) -lstdc++ -O3

.PHONY : all
all : $(addsuffix .exe, $(basename $(wildcard *.cpp)))

compile_fail%.exe : compile_fail%.cpp
	@echo Expecting fail for compiling $< ...
	@if ${CXX} -o $@ ${CXXFLAGS} $<; then \
		echo !!Fail!! Compiling successed unexpected 1>&2; \
		false; \
	else \
		echo !!OK!! Compiling Failed as expected; \
	fi

%.exe : %.cpp
	${CXX} -o $@ ${CXXFLAGS} $<


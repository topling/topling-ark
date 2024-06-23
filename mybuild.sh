#!/bin/bash

#set -x
set -e

if [ -z "$j" ]; then
	j=`grep 'processor' /proc/cpuinfo | wc -l`
fi

if [ "${qtoolset}" = icc ]; then
	export AR=/home/s/apps/icc/bin/xiar
	export LD="/home/s/apps/icc/bin/icpc -gcc-name=/opt/gcc-4.7.2/bin/gcc"
	export CC="/home/s/apps/icc/bin/icc"
	export CXX="/home/s/apps/icc/bin/icc"
	export RE2_CXX=${CXX}
	export CXXFLAGS=-gcc-name=/opt/bin/g++
	export LD_LIBRARY_PATH=/home/s/apps/icc/lib/intel64:$LD_LIBRARY_PATH
elif [ "${qtoolset}" = "g++" -o "${qtoolset}" = gcc ]; then
	qtoolset="g++"
	export LD="/opt/bin/g++"
	export CC="/opt/bin/gcc"
	export CXX="/opt/bin/g++"
	export RE2_CXX=g++
elif [ "${qtoolset}" = "g++-4.9" -o "${qtoolset}" = gcc-4.9 ]; then
	qtoolset="g++"
	export LD="/home/leipeng/opt/gcc-4.9/bin/g++"
	export CC="/home/leipeng/opt/gcc-4.9/bin/gcc"
	export CXX="/home/leipeng/opt/gcc-4.9/bin/g++"
	export RE2_CXX=g++
elif [ "${qtoolset}" = "clang" ]; then
	export LD="/home/leipeng/osc/llvm-build/Release/bin/clang++"
	export CC="/home/leipeng/osc/llvm-build/Release/bin/clang"
	export CXX="/home/leipeng/osc/llvm-build/Release/bin/clang"
	export RE2_CXX=g++
elif [ "${qtoolset}" = "clanga" ]; then
	export LD="/home/leipeng/osc/llvm-build/Release+Asserts/bin/clang++"
	export CC="/home/leipeng/osc/llvm-build/Release+Asserts/bin/clang"
	export CXX="/home/leipeng/osc/llvm-build/Release+Asserts/bin/clang"
	export RE2_CXX=g++
else
	if [ -n "${qtoolset}" ]; then
		echo Not Supported toolset: "${qtoolset}" 1>&2
	fi
	echo env var qtoolset must be set to icc or gcc  1>&2
	exit
fi
export INCS=-I/opt/include
export BOOST_INC=/home/leipeng/opt/include
export LIBBOOST="/home/leipeng/opt/lib/libboost_thread.a \
				 /home/leipeng/opt/lib/libboost_date_time.a \
				 /home/leipeng/opt/lib/libboost_system.a "

scanbuild=/home/leipeng/osc/llvm-src/tools/clang/tools/scan-build/scan-build
analyzer=/home/leipeng/osc/llvm-build/Release+Asserts/bin/clang

if [ "$2" = 1 ]; then
   	rm -rf build/${qtoolset}-*
	make -C tools/automata clean
	make -C tools/regex clean
	make -C test/fsa clean
fi

#$scanbuild --use-c++=${CXX} --use-cc=${CC} --use-analyzer $analyzer make -j$j CXXFLAGS="-I/opt/include $CXXFLAGS"
make -j$j CXXFLAGS="-I/opt/include $CXXFLAGS"


#!/bin/sh
set -e
set -x

if [ "$1" != "pkg" -a "$1" != "tgz" -a "$1" != "scp" -a "$1" != "oss" ]; then
	echo usage $0 "pkg | tgz | scp | oss"
	exit 1
fi

if [ -z "$BMI2" ]; then
	BMI2=`sh ./cpu_has_bmi2.sh`
fi

if [ -z "$CPU_NUM" ]; then
	CPU_NUM=1
fi
tmpfile=`mktemp`
COMPILER=`g++ tools/configure/compiler.cpp -o ${tmpfile}.exe && ${tmpfile}.exe && rm -f ${tmpfile}*`
UNAME_MachineSystem=`uname -m -s | sed 's:[ /]:-:g'`
echo PWD=`pwd`
if [ -z "$CPU" ]; then
  make WITH_BMI2=$BMI2 DFADB_TARGET=DfaDB PKG_WITH_DBG=1 PKG_WITH_DEP=1 -j$CPU_NUM $1 CFLAGS="${FLAGS}"
else
  make WITH_BMI2=$BMI2 DFADB_TARGET=DfaDB PKG_WITH_DBG=1 PKG_WITH_DEP=1 -j$CPU_NUM $1 CFLAGS="${FLAGS}" CPU=$CPU
fi

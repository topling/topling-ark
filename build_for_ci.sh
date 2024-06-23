#!/bin/sh

set -e
set -x

CompilerList=(
    /opt/g++-4.8
#    /opt/g++-4.9
#    /opt/g++-5.3
    /opt/g++-5.4
#    /opt/g++-6.1
#    /opt/g++-6.2
#    /opt/g++-6.3
#    /opt/g++-7.1
#    /opt/g++-7.2
)

function build() {
    PKG="pkg"
    BMI2=0

    #if [ -z "$BMI2" ]; then
    #    BMI2=`sh ./cpu_has_bmi2.sh`
    #fi

    if [ `uname` == Darwin ]; then
        CPU_NUM=`sysctl -n machdep.cpu.thread_count`
    else
        CPU_NUM=`nproc`
    fi

    CPU=-march=corei7

    tmpfile=`mktemp`
    COMPILER=`g++ tools/configure/compiler.cpp -o ${tmpfile}.exe && ${tmpfile}.exe && rm -f ${tmpfile}*`
    FLAGS="-include `pwd`/tools/configure/glibc_memcpy_fix.h "
    UNAME_MachineSystem=`uname -m -s | sed 's:[ /]:-:g'`
    echo PWD=`pwd`

    make PKG_WITH_STATIC=1 WITH_BMI2=$BMI2 DFADB_TARGET=DfaDB PKG_WITH_DBG=1 PKG_WITH_DEP=1 -j$CPU_NUM $PKG CXXFLAGS="${FLAGS}" CFLAGS="${FLAGS}" CPU=$CPU
}

OLD_PATH=$PATH
OLD_LD_LIBRARY_PATH=$LD_LIBRARY_PATH

for Compiler in ${CompilerList[@]}
do
    if test -e $Compiler/bin; then
        export PATH=$Compiler/bin:$OLD_PATH
        export LD_LIBRARY_PATH=$Compiler/lib64:$OLD_LD_LIBRARY_PATH
        #if [[ $Compiler != clang++ ]]; then
        #    CPU_NUM=$cpuNum CPU=-march=corei7 BMI2=1 \
        #        sh ../terark/deploy.sh $@
        #fi
        build
    else
        echo Not found compiler: $Compiler
    fi
done
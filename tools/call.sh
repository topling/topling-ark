#!/bin/sh

bindir=`dirname $1`
libdir=`cd $bindir/../lib; pwd`
CXX=`echo $bindir | sed 's:.*x86_64-\(.*\)-bmi2-.*:\1:'`
export LD_LIBRARY_PATH=$libdir:/opt/$CXX/lib64:$LD_LIBRARY_PATH

$@

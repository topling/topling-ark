#!/bin/sh

set -e
#set -x

TERARK_ROOT=../../..

export LD_LIBRARY_PATH=$TERARK_ROOT/lib:$LD_LIBRARY_PATH

rm -f *keys*

echo ab              >> akeys.txt # 0
echo abcd            >> akeys.txt # 1
echo abcdef          >> akeys.txt # 2
echo abcdefghijk     >> akeys.txt # 3
echo abcdefghikj-lmn >> bkeys.txt # 4
echo opq             >> bkeys.txt # 5
echo rst             >> bkeys.txt # 6
echo rst123          >> bkeys.txt # 7
echo uvw             >> bkeys.txt # 8
echo uvw123          >> bkeys.txt # 9

cat akeys.txt | $TERARK_ROOT/tools/fsa/dbg/dawg_build.exe -q -O keys.dawg.a -o keys.ldawg.a
cat bkeys.txt | $TERARK_ROOT/tools/fsa/dbg/dawg_build.exe -q -O keys.dawg.b -o keys.ldawg.b

$TERARK_ROOT/samples/fsa/abstract_api/dbg/idx2word -i 'keys.dawg.a;keys.dawg.b' 0 1 2 3 4 5 6 7 8 9
$TERARK_ROOT/samples/fsa/abstract_api/dbg/word2idx -i 'keys.dawg.a;keys.dawg.b' `cat akeys.txt bkeys.txt` none-existing

$TERARK_ROOT/samples/fsa/abstract_api/dbg/idx2word -i 'keys.ldawg.a;keys.ldawg.b' 0 1 2 3 4 5 6 7 8 9
$TERARK_ROOT/samples/fsa/abstract_api/dbg/word2idx -i 'keys.ldawg.a;keys.ldawg.b' `cat akeys.txt bkeys.txt` none-existing


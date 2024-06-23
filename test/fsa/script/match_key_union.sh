#!/bin/sh

set -e
#set -x

TERARK_ROOT=../../..
export LD_LIBRARY_PATH=$TERARK_ROOT/lib:$LD_LIBRARY_PATH

rm -f *keyval*

echo ab              + 0  >> akeyval.txt # 0
echo ab              + 01 >> akeyval.txt # 0
echo abcd            + 1  >> akeyval.txt # 1
echo abcd            + 11 >> akeyval.txt # 1
echo abcdef          + 2  >> akeyval.txt # 2
echo abcdefghijk     + 3  >> akeyval.txt # 3
echo abcdefghijk-lmn + 4  >> bkeyval.txt # 4
echo opq             + 5  >> bkeyval.txt # 5
echo rst             + 6  >> bkeyval.txt # 6
echo rst123          + 7  >> bkeyval.txt # 7
echo uvw             + 8  >> bkeyval.txt # 8
echo uvw123          + 9  >> bkeyval.txt # 9

cat akeyval.txt | $TERARK_ROOT/tools/fsa/dbg/adfa_build.exe -q -O keyval.fdfa.a -o keyval.ldfa.a
cat bkeyval.txt | $TERARK_ROOT/tools/fsa/dbg/adfa_build.exe -q -O keyval.fdfa.b -o keyval.ldfa.b

$TERARK_ROOT/samples/fsa/abstract_api/dbg/match_key '-d ' -i keyval.fdfa.a 'abcdefghijk-lmnopq' opqrst rst1234
$TERARK_ROOT/samples/fsa/abstract_api/dbg/match_key '-d ' -i keyval.fdfa.b 'abcdefghijk-lmnopq' opqrst rst1234
$TERARK_ROOT/samples/fsa/abstract_api/dbg/match_key '-d ' -i keyval.ldfa.a 'abcdefghijk-lmnopq' opqrst rst1234
$TERARK_ROOT/samples/fsa/abstract_api/dbg/match_key '-d ' -i keyval.ldfa.b 'abcdefghijk-lmnopq' opqrst rst1234

$TERARK_ROOT/samples/fsa/abstract_api/dbg/match_key '-d ' -i 'keyval.fdfa.a;keyval.fdfa.b' 'abcdefghijk-lmnopq' opqrst rst1234
$TERARK_ROOT/samples/fsa/abstract_api/dbg/match_key '-d ' -i 'keyval.ldfa.a;keyval.ldfa.b' 'abcdefghijk-lmnopq' opqrst rst1234

$TERARK_ROOT/samples/fsa/abstract_api/dbg/match_key -l '-d ' -i keyval.fdfa.a 'abcdefghijk-lmnopq' opqrst rst1234
$TERARK_ROOT/samples/fsa/abstract_api/dbg/match_key -l '-d ' -i keyval.fdfa.b 'abcdefghijk-lmnopq' opqrst rst1234
$TERARK_ROOT/samples/fsa/abstract_api/dbg/match_key -l '-d ' -i keyval.ldfa.a 'abcdefghijk-lmnopq' opqrst rst1234
$TERARK_ROOT/samples/fsa/abstract_api/dbg/match_key -l '-d ' -i keyval.ldfa.b 'abcdefghijk-lmnopq' opqrst rst1234

$TERARK_ROOT/samples/fsa/abstract_api/dbg/match_key -l '-d ' -i 'keyval.fdfa.a;keyval.fdfa.b' 'abcdefghijk-lmnopq' opqrst rst1234
$TERARK_ROOT/samples/fsa/abstract_api/dbg/match_key -l '-d ' -i 'keyval.ldfa.a;keyval.ldfa.b' 'abcdefghijk-lmnopq' opqrst rst1234


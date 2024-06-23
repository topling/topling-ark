#!/bin/sh

set -e
set -x

if [ -z "$CXX" ]; then
	CXX=g++
fi
if [ -z "$BMI2" ]; then
	BMI2=1
fi
dir=$(cd $(dirname $0);pwd)
TERARK_HOME=$(cd $dir/../../..;pwd)
tmpfile=$(mktemp compiler-XXXXXX)
COMPILER=$(cd $TERARK_HOME && ${CXX} tools/configure/compiler.cpp -o ${tmpfile}.exe && ./${tmpfile}.exe && rm -f ${tmpfile}*)

export LD_LIBRARY_PATH=$TERARK_HOME/build/Linux-x86_64-${COMPILER}-bmi2-$BMI2/lib:$LD_LIBRARY_PATH

find ../.. -name '*pp' | xargs sed 's/\s\+/\n/g' > alltoken.txt

awk 'length($0)>0{printf("%s\t%d\n", $1, length($1))}' alltoken.txt > alltoken_len.txt

LC_ALL=C sort -u alltoken.txt | sed '/^$/d' > alltoken.sorted.uniq

FSA_TOOLS=$dir/../../../tools/fsa/dbg
FSA_SAMPLES=$dir/../../../samples/fsa/abstract_api/dbg

$FSA_TOOLS/adfa_build.exe -O alltoken_len.dfa -U alltoken_len.louds alltoken_len.txt 
$FSA_TOOLS/adfa_build.exe -U alltoken_len.mmap                      alltoken_len.txt 
$FSA_TOOLS/dawg_build.exe -O alltoken.dawg.mmap                     alltoken.txt 

$FSA_SAMPLES/match_key.exe   -i alltoken_len.dfa     int uint long byte char automata > out.read.read.match_key
$FSA_SAMPLES/match_key.exe   -i alltoken_len.mmap    int uint long byte char automata > out.read.mmap.match_key
$FSA_SAMPLES/match_key.exe   -i alltoken_len.mmap    int uint long byte char automata > out.mmap.mmap.match_key
cat alltoken_len.mmap| $FSA_SAMPLES/match_key.exe    int uint long byte char automata > out.mmap.mmap.match_key

$FSA_TOOLS/ac_build.exe -O alltoken.auac -d alltoken.daac -m -e 1 alltoken.txt 
echo int uint long byte char automata | $FSA_SAMPLES/ac_scan.exe -i alltoken.auac  > out.read.mmap.ac_scan.auac
echo int uint long byte char automata | $FSA_SAMPLES/ac_scan.exe -i alltoken.daac  > out.read.mmap.ac_scan.daac

$FSA_SAMPLES/idx2word.exe 1 10 20 30 40 50 60 70 80 90 100 < alltoken.dawg.mmap  > out.read.mmap.dawg.idx2word
$FSA_SAMPLES/word2idx.exe int uint long byte char automata < alltoken.dawg.mmap  > out.read.mmap.dawg.word2idx


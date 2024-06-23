#!/bin/sh

set -x

rm -rf terark-fsa-win-x86
rm -rf terark-fsa-win-x64
mkdir -p terark-fsa-win-x86
mkdir -p terark-fsa-win-x64

cp Release/*.dll terark-fsa-win-x86
cp Release/*.exe terark-fsa-win-x86

rm terark-fsa-win-x86/valvec_sfinae.exe
rm terark-fsa-win-x86/PragmaPackBitField.exe
rm terark-fsa-win-x86/lazy_union_dfa.exe
mv terark-fsa-win-x86/aho_corasick.exe  terark-fsa-win-x86/ac_bench.exe

tar cjf terark-fsa-win-x86.tar.bz2 terark-fsa-win-x86

cp x64/Release/*.dll terark-fsa-win-x64
cp x64/Release/*.exe terark-fsa-win-x64

rm terark-fsa-win-x64/valvec_sfinae.exe
rm terark-fsa-win-x64/PragmaPackBitField.exe
rm terark-fsa-win-x64/lazy_union_dfa.exe
mv terark-fsa-win-x64/aho_corasick.exe  terark-fsa-win-x64/ac_bench.exe

tar cjf terark-fsa-win-x64.tar.bz2 terark-fsa-win-x64


#!/bin/sh

set -x

rm -rf terark-fsa_all-win-x64
mkdir -p terark-fsa_all-win-x64

cp x64/Release/*.dll terark-fsa_all-win-x64
cp x64/Release/*.exe terark-fsa_all-win-x64

rm terark-fsa_all-win-x64/valvec_sfinae.exe
rm terark-fsa_all-win-x64/PragmaPackBitField.exe
rm terark-fsa_all-win-x64/lazy_union_dfa.exe
mv terark-fsa_all-win-x64/aho_corasick.exe  terark-fsa_all-win-x64/ac_bench.exe

tar cjf terark-fsa_all-win-x64.tar.bz2 terark-fsa_all-win-x64
git log -n 1 > terark-fsa_all-win-x64/package.version.txt
scp terark-fsa_all-win-x64.tar.bz2 root@nark.cc:/var/www/html/download

exit 0

rm -rf terark-fsa_all-win-x86
mkdir -p terark-fsa_all-win-x86
cp Release/*.dll terark-fsa_all-win-x86
cp Release/*.exe terark-fsa_all-win-x86

rm terark-fsa_all-win-x86/valvec_sfinae.exe
rm terark-fsa_all-win-x86/PragmaPackBitField.exe
rm terark-fsa_all-win-x86/lazy_union_dfa.exe
mv terark-fsa_all-win-x86/aho_corasick.exe  terark-fsa_all-win-x86/ac_bench.exe

tar cjf terark-fsa_all-win-x86.tar.bz2 terark-fsa_all-win-x86



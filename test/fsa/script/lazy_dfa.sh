#!/usr/bin/bash

set -e
selfdir=$(cd $(dirname $0); pwd)
datadir=$(cd $selfdir/../../../nlp/data/orig; pwd)
terarkRoot=$(cd $selfdir/../../..; pwd)

cat $datadir/word-pinyin-{1,2}.txt $datadir/query-correction.txt |
	env LC_ALL=C sort -u > words.txt

rm -rf splitWords
mkdir -p splitWords
split -C 4096 -a 3 -d words.txt splitWords/x

function BuildLoudsDFA() {
	txt=$1
	$terarkRoot/tools/fsa/rls/adfa_build.exe -q -O $1.dfa -U $1.udfa $txt
}

function BuildRecursiveLoudsTrie() {
	txt=$1
	$terarkRoot/tools/fsa/rls/rptrie_build.exe -o $1.rlt $txt
}

function RunTest() {
	cmd=$1
	ext=$2
	for f in splitWords/x???
	do
		$cmd $f
	done
	cd splitWords
	$terarkRoot/samples/fsa/abstract_api/rls/simple_unzip "`echo x*.$ext`" > ../words.txt.$ext
	cd ..
	diff words.txt words.txt.$ext
}

RunTest BuildLoudsDFA dfa
RunTest BuildRecursiveLoudsTrie rlt

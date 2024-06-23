#!/bin/bash

export LANG=zh_CN.utf8
export LC_ALL=zh_CN.utf8

set -x

sed -n 's/^\(.\) .*（\(.*\)）/\1\t\2/p' orig/origin-cjk-pinyin-42856.txt \
	> norm/cjk-pinyin-42856-merged.txt

function merged_to_disperse() {
	perl -lane 'for($i=1; $i<@F; ++$i) { printf("%s\t%s\n", $F[0], $F[$i]); }' $@
}

merged_to_disperse norm/cjk-pinyin-42856-merged.txt > norm/cjk-pinyin-42856-disperse.txt
merged_to_disperse orig/good-gbk-pinyin-merged.txt  > norm/good-gbk-pinyin-disperse.txt
sed 's/[0-9]//g' norm/good-gbk-pinyin-disperse.txt | sort -u > norm/good-gbk-pinyin-no-yindiao.txt
sed 's/[0-9]//g' norm/cjk-pinyin-42856-disperse.txt | sort -u > norm/cjk-pinyin-42856-no-yindiao.txt

perl -ane 'if (@F>2) { print($_); }' \
	norm/cjk-pinyin-42856-merged.txt > norm/cjk-duoyin-with-yindiao.txt

perl -ne \
   'chomp;
	s/\d\b//g;
	@F = split(" ");
	%seen = ();
	@uniq = ();
	foreach $item (@F) {
		if (!$seen{$item}) {
			$seen{$item}=1;
			push(@uniq, $item);
		}
	}
	if (@uniq>2) {
		for ($i=0; $i<@uniq; ++$i) {
			if ($i<@uniq-1) {
				printf("%s ", $uniq[$i]);
			} else {
				printf("%s\n", $uniq[$i]);
			}
		}
   	}' \
	norm/cjk-pinyin-42856-merged.txt > norm/cjk-duoyin-none-yindiao.txt


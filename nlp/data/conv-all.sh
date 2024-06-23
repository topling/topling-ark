#!/bin/sh

set -x

mkdir -p norm

perl conv-29591.pl orig/pinyin-hanzi-29591.txt | \
perl pinyin_hanzi-to-hanzi_pinyin.pl > norm/hanzi-pinyin-29591.txt


# orig/pinyin-hanzi-28194.txt 中多音字貌似是合适的
# 其他 汉字-拼音 表 中 的 多音字 有很多 几乎用不到的
#    例如 蛇: she yi tuo chi, 字典能找到的只有 she yi
#    那些几乎用不到的可能是方言或模糊音吧
perl pinyin_hanzi-to-hanzi_pinyin.pl orig/pinyin-hanzi-28194.txt > norm/hanzi-pinyin-28194.txt
perl pinyin_hanzi-to-hanzi_pinyin.pl orig/pinyin-hanzi-daquan.txt > norm/hanzi-pinyin-daquan.txt

# trans_pinyin.sed is not used, use trans_pinyin.pl
# orig/from-unihan-reading is converted from
#   http://www.unicode.org/Public/UCD/latest/ucd/Unihan.zip/Unihan_Readings.txt
perl trans_pinyin.pl orig/from-unihan-reading.txt > norm/from-unihan-reading.txt

sh cjk-convert.sh


#!/bin/sh

cp ../../src/terark/objbox.* ../../src/terark/ptree.* ~/qihoo/cs/trunk/cloudmark/php
mkdir -p ~/qihoo/cs/trunk/cloudmark/php/test
cp *.php load.cpp sample.cpp ~/qihoo/cs/trunk/cloudmark/php/test
sed -i 's/namespace\s\+terark/namespace php/' ~/qihoo/cs/trunk/cloudmark/php/*.{cpp,hpp}
sed -i 's/terark/php/g' ~/qihoo/cs/trunk/cloudmark/php/test/*.cpp


#!/bin/sh

terarkHome=`dirname $0`

find src test -type f | xargs -n1 | (
set -x
while read fname
do
	cp -P "${terarkHome}/$fname" "$fname"
done
)

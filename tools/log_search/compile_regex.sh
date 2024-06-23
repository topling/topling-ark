#!/bin/sh

if [ "x$rbuild" = "x" ]; then
	if    [ -e ../regex/rls/regex_build.exe ]; then
		rbuild=../regex/rls/regex_build.exe
	elif  [ -e ../bin/regex_build.exe ]; then
		rbuild=../bin/regex_build.exe
	else
		echo "can't find regex_build.exe"
	   	echo "please set env var 'rbuild' as fullpath of your regex_build.exe"
	fi
fi

"$rbuild" -o fields.vmdfa -O fields.dfa -b fields.binmeta -I -ss fields.regex


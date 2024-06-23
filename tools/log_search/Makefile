
include ../fsa/Makefile

fields.dfa fields.vm fields.binmeta RegexCate.hpp RegexCate.inl: fields.regex
	../regex/rls/regex_build.exe -o fields.vmdfa -O fields.dfa -b fields.binmeta -I -ss -E RegexCate fields.regex
	mv RegexCate.cpp RegexCate.inl

${DBG_DIR}/log_indexer.o : RegexCate.inl
${RLS_DIR}/log_indexer.o : RegexCate.inl

${DBG_DIR}/default_log_parser.o : RegexCate.inl
${RLS_DIR}/default_log_parser.o : RegexCate.inl


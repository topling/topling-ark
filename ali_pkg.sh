#/usr/bin/bash

set -e

rm -rf ali_pkg
mkdir -p ali_pkg/tools/configure
mkdir -p ali_pkg/tools/codegen
cp tools/configure/* ali_pkg/tools/configure
cp tools/codegen/fuck_bom_out.cpp ali_pkg/tools/codegen

mkdir -p ali_pkg/3rdparty
cp -ral 3rdparty/base64           ali_pkg/3rdparty
cp -ral 3rdparty/zstd ali_pkg/3rdparty

cp -al gen_env_conf.sh ali_pkg/
cp -al cpu_*.sh        ali_pkg/
cp -al obfuscate.pl    ali_pkg/
cp -al README.md       ali_pkg/
#cp -al ali.Makefile    ali_pkg/Makefile

cp -ral src            ali_pkg/

for f in src/terark/{fsa,zbs}/*.?pp
do
	rm ali_pkg/$f
	sed -b    '/TerarkFSA_HighPrivate/,/#endif.*TerarkFSA_HighPrivate/d' $f > ali_pkg/$f
	sed -b -i '/TerarkFSA_With_DfaKey/,/#endif.*TerarkFSA_With_DfaKey/d'      ali_pkg/$f
done

rm -rf ali_pkg/src/terark/bdb_util
rm -rf ali_pkg/src/terark/c
rm -rf ali_pkg/src/terark/inet
rm -rf ali_pkg/src/terark/rpc
rm -rf ali_pkg/src/terark/todo
rm -rf ali_pkg/src/terark/ghost
rm -rf ali_pkg/src/terark/zsrch
rm     ali_pkg/src/terark/trb_cxx.*
rm     ali_pkg/src/terark/objbox.*
rm     ali_pkg/src/terark/ptree.*
rm     ali_pkg/src/terark/heap_ext.*
rm     ali_pkg/src/terark/replace_select_sort.*
rm     ali_pkg/src/terark/io/BzipStream.*
rm     ali_pkg/src/terark/io/GzipStream.*
rm     ali_pkg/src/terark/util/ini_parser.*

rm -rf ali_pkg/src/terark/fsa/discard
rm -rf ali_pkg/src/terark/fsa/re2

rm  ali_pkg/src/terark/fsa/ppi/accept.hpp
rm  ali_pkg/src/terark/fsa/ppi/adfa_minimize.hpp
rm  ali_pkg/src/terark/fsa/ppi/dfa_const_methods_use_walk.hpp
rm  ali_pkg/src/terark/fsa/ppi/dfa_deprecated_edit_distance_algo.hpp
rm  ali_pkg/src/terark/fsa/ppi/dfa_hopcroft.hpp
rm  ali_pkg/src/terark/fsa/ppi/dfa_methods_calling_swap*
rm  ali_pkg/src/terark/fsa/ppi/dfa_mutation_virtuals_basic.hpp
rm  ali_pkg/src/terark/fsa/ppi/dfa_mutation_virtuals_extra.hpp
rm  ali_pkg/src/terark/fsa/ppi/dfa_reverse*
rm  ali_pkg/src/terark/fsa/ppi/dfa_set_op.hpp
rm  ali_pkg/src/terark/fsa/ppi/dfa_using_template_base.hpp
rm  ali_pkg/src/terark/fsa/ppi/fast_state.hpp
rm  ali_pkg/src/terark/fsa/ppi/flat_dfa_data_io.hpp
rm  ali_pkg/src/terark/fsa/ppi/for_each_suffix.hpp
rm  ali_pkg/src/terark/fsa/ppi/for_each_word.hpp
rm  ali_pkg/src/terark/fsa/ppi/linear_dfa_inclass.hpp
rm  ali_pkg/src/terark/fsa/ppi/linear_dfa_tmpl_inst.hpp
rm  ali_pkg/src/terark/fsa/ppi/linear_dfa_typedef.hpp
rm  ali_pkg/src/terark/fsa/ppi/make_edit_distance_dfa.hpp
rm  ali_pkg/src/terark/fsa/ppi/match_key.hpp
rm  ali_pkg/src/terark/fsa/ppi/match_path.hpp
rm  ali_pkg/src/terark/fsa/ppi/match_pinyin.hpp
rm  ali_pkg/src/terark/fsa/ppi/match_prefix.hpp
rm  ali_pkg/src/terark/fsa/ppi/nfa_algo.hpp
rm  ali_pkg/src/terark/fsa/ppi/nfa_mutation_algo.hpp
rm  ali_pkg/src/terark/fsa/ppi/packed_state.hpp
rm  ali_pkg/src/terark/fsa/ppi/path_zip.hpp
rm  ali_pkg/src/terark/fsa/ppi/pool_dfa_mmap.hpp
rm  ali_pkg/src/terark/fsa/ppi/post_order.hpp
rm  ali_pkg/src/terark/fsa/ppi/quick_dfa_common.hpp
rm  ali_pkg/src/terark/fsa/ppi/tpl_for_each_word.hpp
rm  ali_pkg/src/terark/fsa/ppi/tpl_match_key.hpp
rm  ali_pkg/src/terark/fsa/ppi/trie_map_state.hpp
rm  ali_pkg/src/terark/fsa/ppi/virtual_suffix_cnt.hpp

rm  ali_pkg/src/terark/fsa/aho_corasick*
rm  ali_pkg/src/terark/fsa/automata*.?pp
rm  ali_pkg/src/terark/fsa/auto_gen_anti_pirate.inl
rm  ali_pkg/src/terark/fsa/base_ac.?pp
rm  ali_pkg/src/terark/fsa/create_regex_dfa*
rm  ali_pkg/src/terark/fsa/dawg*
rm  ali_pkg/src/terark/fsa/dense*
rm  ali_pkg/src/terark/fsa/deprecated_edit_distance_matcher.*
rm  ali_pkg/src/terark/fsa/dfa_algo.?pp
rm  ali_pkg/src/terark/fsa/dfa_with_register*
rm  ali_pkg/src/terark/fsa/fsa_for_union_dfa*
rm  ali_pkg/src/terark/fsa/full_dfa.hpp
rm  ali_pkg/src/terark/fsa/dict_trie*
rm  ali_pkg/src/terark/fsa/hopcroft*
rm  ali_pkg/src/terark/fsa/iterator_to_byte_stream.hpp
rm  ali_pkg/src/terark/fsa/linear_dawg*
rm  ali_pkg/src/terark/fsa/linear_dfa*
rm  ali_pkg/src/terark/fsa/louds_dfa*
rm  ali_pkg/src/terark/fsa/mre_ac*
rm  ali_pkg/src/terark/fsa/mre_dawg*
rm  ali_pkg/src/terark/fsa/mre_delim*
rm  ali_pkg/src/terark/fsa/mre_match*
rm  ali_pkg/src/terark/fsa/mre_tmplinst*
rm  ali_pkg/src/terark/fsa/nfa.hpp
rm  ali_pkg/src/terark/fsa/onfly_minimize.hpp
rm  ali_pkg/src/terark/fsa/pinyin_dfa*
rm  ali_pkg/src/terark/fsa/power_set*
rm  ali_pkg/src/terark/fsa/quick_dfa*
rm  ali_pkg/src/terark/fsa/squick_dfa*
rm  ali_pkg/src/terark/fsa/string_as_dfa*
rm  ali_pkg/src/terark/fsa/suffix_dfa*
rm  ali_pkg/src/terark/fsa/virtual_machine_dfa*
rm  ali_pkg/src/terark/fsa/zip.hpp
rm  ali_pkg/src/terark/zbs/suffix_array_trie.?pp

if [ "$1" = "obfuscate" ]; then
	rm -rf obfuscated-terark/
	cd ali_pkg
	make obfuscate -j32
	cd ../
	tar czf obfuscated-terark.tgz obfuscated-terark/
else
	tar czf ali_pkg.tgz ali_pkg
fi


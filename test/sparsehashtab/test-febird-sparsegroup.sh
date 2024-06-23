#
if [ ! -e Makefile  ]; then
        ./configure CXX=/opt/gcc-4.4.3/bin/g++ CC=/opt/gcc-4.4.3/bin/g++ 'CPP=/opt/gcc-4.4.3/bin/gcc -E' 'CXXCPP=/opt/gcc-4.4.3/bin/g++ -E' 'CXXFLAGS=-mpopcnt -g -O2' || exit
        #./configure 'CXXFLAGS=-mpopcnt -g -O2' "$@" || exit
fi
make clean && ( make -j8 || exit )
mv time_hash_map time_hash_map.google
mv hashtable_unittest hashtable_unittest.google
mv sparsetable_unittest sparsetable_unittest.google

mv src/google/sparsetable src/google/sparsetable.google || exit
cp sparsetable src/google || exit
make clean
if ! make -j8; then
        echo compile failed, remove -mpopcnt and retry
        ./configure 'CXXFLAGS=-g -O2' "$@" || exit
fi
# restore google
mv src/google/sparsetable.google src/google/sparsetable

mv time_hash_map time_hash_map.terark
mv hashtable_unittest hashtable_unittest.terark
mv sparsetable_unittest sparsetable_unittest.terark

function unittest
{
        if ! ./$1 > $1.out; then
                echo unittest $1 failed
                exit
        fi
}
echo "===============" run google original version "=============="
set -x
unittest sparsetable_unittest.google
unittest hashtable_unittest.google
./time_hash_map.google 1000000 > time_res.google

echo "===============" run terark optimized version "=============="
unittest sparsetable_unittest.terark
unittest hashtable_unittest.terark
./time_hash_map.terark 1000000 > time_res.terark

# compare result
if which vimdiff 2> /dev/null; then
        vimdiff time_res.google time_res.terark
else
        echo result of time_hash_map are in time_res.*
fi



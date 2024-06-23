#!/bin/sh

sed -n '/begin sed gen valvec32/,/end sed gen valvec32/p' src/terark/valvec.hpp > src/terark/valvec32.hpp
sed -i -e 's/\<valvec\>/valvec32/g' \
       -e 's/\<size_t n;/uint32_t n;/g' \
       -e 's/\<size_t c;/uint32_t c;/g' \
       src/terark/valvec32.hpp
sed -i '/sed gen valvec32/d' src/terark/valvec32.hpp
sed -i '/DONT DELETE OR CHANGE THIS LINE/i \\nnamespace terark {\n' src/terark/valvec32.hpp
sed -i '/namespace std/i \} // namespace terark\n' src/terark/valvec32.hpp
sed -i '1 i\#pragma once\n// generated from valvec.hpp by sed-gen-valvec32.bash\n#include "valvec.hpp"' \
       src/terark/valvec32.hpp

# Introduction

I am the author, my github id is [rockeet](https://github.com/rockeet).

The file names & namespace were `terark` many years ago, we did not change `terark` to `topling`.

Topling-Ark contains libraries:
- core library
  - hash_strmap & gold_hash_map ...
  - serialization, rpc
  - succinct, faster than [sdsl-lite](https://github.com/simongog/sdsl-lite)
- World fastest & smallest FSA(Finite State Automata) library
  - NestLoudsTrie: can search on zipped binary string keys, smaller than bzip2(non-searchable)
  - AC(Aho-Corasick) Automata: small & build fast & search fast
  - Virtual Machine DFA, dynamic DFA, ...
  - Hopcroft DFA minimization algo: the fastest implementation
  - DAWG(Directed Acyclic Word Graph): the fastest & the smallest, 20x faster & 20x smaller than the paper
  - Multi RegEx matching algo, faster & smaller than hyperscan for millions+ of RegEx
  - Multi PinYin algo([demo](https://nark.cc)), **100x** smaller than traditional algos
- Tools for building FSA databases
- Others...

These algorithms used in ToplingDB contains:
- Index algorithms
  - cspp trie(contained in FSA library)
  - composite uint index
  - scgt
  - ...
- Value store algorithms
  - DictZipBlobStore
  - EntropyZipBlobStore
  - MixedLenBlobStore
  - ...

## Complile
1. Install libaio-dev lib-uring first if you dont have it yet
2. `build.sh` will do the rest work

## Unit Tests

## Integration
TODO

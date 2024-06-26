#define _CRT_SECURE_NO_WARNINGS
#define _SCL_SECURE_NO_WARNINGS
#include <terark/util/function.hpp>
#include <terark/util/linebuf.hpp>
#include <terark/util/profiling.hpp>
#include <terark/hash_strmap.hpp>
#include <terark/fsa/cspptrie.inl>
#include <terark/fsa/nest_trie_dawg.hpp>
#include <getopt.h>
#include <random>

using namespace terark;

template<class DFA>
void test_empty_dfa(const DFA& dfa) {
    ADFA_LexIteratorUP iterU(dfa.adfa_make_iter(initial_state));
    auto iter = iterU.get();
    TERARK_VERIFY(!iter->seek_begin());
    TERARK_VERIFY(!iter->seek_end());
    TERARK_VERIFY(!iter->seek_lower_bound(""));
    TERARK_VERIFY(!iter->seek_lower_bound("1"));
    TERARK_VERIFY(!iter->seek_lower_bound("2"));
    TERARK_VERIFY(!iter->seek_lower_bound("9"));
    TERARK_VERIFY(!iter->seek_lower_bound("\xFF"));
}
template<class Trie, class DawgType>
void test_empty_dfa(const NestTrieDAWG<Trie, DawgType>& dfa) {}

template<class DFA, class Inserter>
void unit_test_run(Inserter insert);
void unit_test();
void run_benchmark();
void test_run_impl(const MatchingDFA& dfa, const hash_strmap<>& strVec);

void usage(const char* prog) {
    fprintf(stderr, R"EOS(usage: %s Options

Options:
    -a
       test Automata<State32>
    -n
       test NLT
    -b
       run benchmark
    -f dfa_file
       load dfa_file for test
    -I
       with benchmark for iterator create
    -?
)EOS", prog);
}

bool test_nlt = false;
bool bench_iter_create = false;
const char* dfa_file = NULL;
const char* fname = "fab-data.txt";
FILE* fp = NULL;

int main(int argc, char* argv[]) {
    bool benchmark = false;
    for (int opt = 0; (opt = getopt(argc, argv, "bnIhf:")) != -1; ) {
        switch (opt) {
        case '?': usage(argv[0]); return 3;
        case 'b':
            benchmark = true;
            break;
        case 'n':
            test_nlt = true;
            break;
        case 'I':
            bench_iter_create = true;
            break;
        case 'f':
            dfa_file = optarg;
            break;
        }
    }
    if (optind < argc) {
        fname = argv[optind];
    }
    fp = fopen(fname, "r");
    if (!fp) {
        fprintf(stderr, "fatal: fopen(%s) = %s\n", fname, strerror(errno));
    }
    if (benchmark) {
        run_benchmark();
    }
    else {
        unit_test();
    }
    return 0;
}

void run_benchmark() {
    MainPatricia trie(sizeof(uint32_t), 1<<20, Patricia::SingleThreadShared);
    hash_strmap<uint32_t> strVec;
    LineBuf line;
    size_t lineno = 0;
    size_t bytes = 0;
    printf("reading file %s ...\n", fname);
    profiling pf;
    auto t0 = pf.now();
    while (line.getline(fp) > 0) {
        line.trim();
        strVec[line]++;
        bytes += line.size();
        lineno++;
        if (lineno % TERARK_IF_DEBUG(1000, 10000) == 0) {
            //printf("lineno=%zd\n", lineno);
            printf("."); fflush(stdout);
        }
    }
    if (strVec.end_i() == 0) {
        printf("input is empty\n");
        return;
    }
    printf("\n");
    auto t1 = pf.now();
    printf("read  input: time = %10.3f, QPS = %10.3f M, TP = %10.3f MB/sec, lines=%zd\n",
            pf.sf(t0,t1), lineno / pf.uf(t0,t1), bytes / pf.uf(t0,t1), lineno);

    bytes = 0;
    for (size_t i = 0; i < strVec.end_i(); ++i) {
        bytes += strVec.key_len(i);
    }

    Patricia::WriterTokenPtr wtoken(new Patricia::WriterToken);
    wtoken->acquire(&trie);
    t0 = pf.now();
    for (size_t i = 0; i < strVec.end_i(); ++i) {
        fstring key = strVec.key(i);
        auto  & val = strVec.val(i);
        TERARK_VERIFY_S(wtoken->insert(key, &val), "%s : %d", key, val);
        TERARK_VERIFY_EQ(wtoken->value_of<uint32_t>(), val);
    }
    wtoken->release();
    trie.set_readonly();
    t1 = pf.now();
    printf("trie insert: time = %10.3f, QPS = %10.3f M, TP = %10.3f MB/sec\n"
        , pf.sf(t0,t1), strVec.end_i()/pf.uf(t0,t1), bytes/pf.uf(t0,t1));

    t0 = pf.now();
    strVec.sort_slow();
    t1 = pf.now();
    printf("svec   sort: time = %10.3f, QPS = %10.3f M, TP = %10.3f MB/sec\n"
        , pf.sf(t0,t1), strVec.end_i()/pf.uf(t0,t1), bytes/pf.uf(t0,t1));

    t0 = pf.now();
    valvec<size_t> shuf(strVec.size(), valvec_no_init());
    for (size_t i = 0; i < shuf.size(); ++i) shuf[i] = i;
    std::shuffle(shuf.begin(), shuf.end(), std::mt19937());
    t1 = pf.now();
    printf("shuf  index: time = %10.3f\n", pf.sf(t0,t1));

    Patricia::IteratorPtr iter(trie.new_iter());
    t0 = pf.now();
    for (size_t i = 0; i < strVec.end_i(); ++i) {
        fstring key = strVec.key(i);
        TERARK_VERIFY_S(iter->seek_lower_bound(key), "%s", key);
    }
    t1 = pf.now();
    printf("lower_bound: time = %10.3f, QPS = %10.3f M, TP = %10.3f MB/sec\n"
        , pf.sf(t0,t1), strVec.end_i()/pf.uf(t0,t1), bytes/pf.uf(t0,t1));

    auto t2 = pf.now();
    for (size_t i = 0; i < strVec.end_i(); ++i) {
        fstring key = strVec.key(i);
        auto    val = strVec.val(i);
        TERARK_VERIFY_S(iter->lookup(key), "%s : %d", key, val);
        TERARK_VERIFY_EQ(iter->value_of<uint32_t>(), val);
    }
    auto t3 = pf.now();
    printf("trie lookup: time = %10.3f, QPS = %10.3f M, TP = %10.3f MB/sec, %8.3f X speed of lower_bound\n"
        , pf.sf(t2,t3), strVec.end_i()/pf.uf(t2,t3), bytes/pf.uf(t2,t3)
        , pf.nf(t0,t1)/pf.nf(t2,t3));

  if (bench_iter_create) {
    t2 = pf.now();
    for (size_t i = 0; i < strVec.end_i(); ++i) {
        Patricia::IteratorPtr iter2(trie.new_iter());
        fstring key = strVec.key(i);
        TERARK_VERIFY_S(iter2->seek_lower_bound(key), "%s", key);
        auto va1 = strVec.val(i);
        auto va2 = iter2->value_of<uint32_t>();
        TERARK_VERIFY_EQ(va1, va2);
    }
    t3 = pf.now();
    printf("NewIter low: time = %10.3f, QPS = %10.3f M, TP = %10.3f MB/sec, %8.3f X speed of lower_bound\n"
        , pf.sf(t2,t3), strVec.end_i()/pf.uf(t2,t3), bytes/pf.uf(t2,t3)
        , pf.nf(t0,t1)/pf.nf(t2,t3));
  }
    t2 = pf.now();
    TERARK_VERIFY(iter->seek_begin());
    for (size_t i = 0; i < strVec.end_i() - 1; ++i) {
        fstring key = strVec.key(i);
        fstring ke2 = iter->word();
        auto    va1 = strVec.val(i);
        auto    va2 = iter->value_of<uint32_t>();
        TERARK_VERIFY_EQ(va1, va2);
        TERARK_VERIFY_S_EQ(key, ke2);
        TERARK_VERIFY(iter->incr());
    }
    TERARK_VERIFY(!iter->incr());
    t3 = pf.now();
    printf("trie   walk: time = %10.3f, QPS = %10.3f M, TP = %10.3f MB/sec, %8.3f X speed of lower_bound\n"
        , pf.sf(t2,t3), strVec.end_i()/pf.uf(t2,t3), bytes/pf.uf(t2,t3)
        , pf.nf(t0,t1)/pf.nf(t2,t3));

    printf("\n------ random ----------------------\n\n");
    fstrvec fstrVec;
    fstrVec.reserve(strVec.end_i());
    fstrVec.reserve_strpool(strVec.total_key_size());
    t2 = pf.now();
    for(size_t i = 0; i < strVec.end_i(); ++i) {
        size_t j = shuf[i];
        fstring key = strVec.key(j);
        fstrVec.push_back(key);
    }
    t3 = pf.now();
    printf("WrtStr Rand: time = %10.3f, QPS = %10.3f M, TP = %10.3f MB/sec, %8.3f X speed of lower_bound seq\n"
        , pf.sf(t2,t3), strVec.end_i()/pf.uf(t2,t3), bytes/pf.uf(t2,t3)
        , pf.nf(t0,t1)/pf.nf(t2,t3));

    auto t4 = pf.now();
    for (size_t i = 0; i < strVec.end_i(); ++i) {
        fstring key = fstrVec[i];
        TERARK_VERIFY_S(iter->seek_lower_bound(key), "%s", key);
    }
    auto t5 = pf.now();
    printf("lower_bound: time = %10.3f, QPS = %10.3f M, TP = %10.3f MB/sec, %8.3f X TIME  of lower_bound seq\n"
        , pf.sf(t4,t5), strVec.end_i()/pf.uf(t4,t5), bytes/pf.uf(t4,t5)
        , pf.nf(t4,t5)/pf.nf(t0,t1));

    t0 = t4;
    t1 = t5;

    t2 = pf.now();
    for (size_t i = 0; i < strVec.end_i(); ++i) {
        fstring key = fstrVec[i];
        TERARK_VERIFY_S(iter->lookup(key), "%s", key);
    }
    t3 = pf.now();
    printf("trie lookup: time = %10.3f, QPS = %10.3f M, TP = %10.3f MB/sec, %8.3f X speed of lower_bound\n"
        , pf.sf(t2,t3), strVec.end_i()/pf.uf(t2,t3), bytes/pf.uf(t2,t3)
        , pf.nf(t0,t1)/pf.nf(t2,t3));

  if (bench_iter_create) {
    t2 = pf.now();
    for (size_t i = 0; i < strVec.end_i(); ++i) {
        ADFA_LexIteratorUP iterp2(trie.adfa_make_iter());
        fstring key = fstrVec[i];
        TERARK_VERIFY_S(iterp2->seek_lower_bound(key), "%s", key);
    }
    t3 = pf.now();
    printf("NewIter low: time = %10.3f, QPS = %10.3f M, TP = %10.3f MB/sec, %8.3f X speed of lower_bound\n"
        , pf.sf(t2,t3), strVec.end_i()/pf.uf(t2,t3), bytes/pf.uf(t2,t3)
        , pf.nf(t0,t1)/pf.nf(t2,t3));
  }
}

template<class NLT>
void unit_nlt() {
    printf("unit_test_run: %s\n\n", BOOST_CURRENT_FUNCTION);
    {
        auto insert = [](NLT& nlt, const hash_strmap<>& strVec) {
            SortableStrVec sVec;
            sVec.reserve(strVec.size(), strVec.total_key_size());
            for (size_t i = 0, n = strVec.end_i(); i < n; i++) {
                fstring key = strVec.key(i);
                sVec.push_back(key);
            }
            NestLoudsTrieConfig conf;
            nlt.build_from(sVec, conf);
        };
        unit_test_run<NLT>(insert);
    }
    rewind(stdin);
    printf("\n");
}

void unit_test() {
    if (dfa_file) {
        std::unique_ptr<MatchingDFA> dfa(MatchingDFA::load_from(dfa_file));
        hash_strmap<> strVec;
        dfa->for_each_word([&](size_t nth, fstring word) {
            TERARK_VERIFY_EQ(strVec.end_i(), nth);
            auto ib = strVec.insert_i(word);
            TERARK_VERIFY(ib.second);
            TERARK_VERIFY_EQ(ib.first, nth);
        });
        if (auto dawg = dfa->get_dawg()) {
            TERARK_VERIFY_EQ(strVec.end_i(), dawg->num_words());
        }
        strVec.i_know_data_is_sorted();
        printf("readed dfa file, keys = %zd\n", strVec.size());
        test_run_impl(*dfa, strVec);
        if (auto pt = dynamic_cast<const MainPatricia*>(dfa.get())) {
            MainPatricia dyna(pt->get_valsize(), pt->mem_size() * 9/8);
            MainPatricia::WriterTokenPtr token(new Patricia::WriterToken());
            token->acquire(&dyna);
            for (size_t i = 0; i < strVec.end_i(); ++i) {
                fstring key = strVec.key(i);
                size_t v = i;
                dyna.insert(key, &v, &*token);
            }
            token->release();
            printf("inserted to MainPatricia, keys = %zd\n", strVec.size());
            test_run_impl(dyna, strVec);
        }
        return;
    }
    if (test_nlt) {
        unit_nlt<NestLoudsTrieDAWG_IL_256_32_FL>();
        unit_nlt<NestLoudsTrieDAWG_IL_256      >();
    }
    {
        printf("unit_test_run: MainPatricia\n\n");
        {
            MainPatricia trie(sizeof(uint32_t));
            MainPatricia::WriterTokenPtr token(new MainPatricia::WriterToken());
            token->acquire(&trie);
            for (uint32_t i = 0; i < 256; ++i) {
                char strkey[9] = "01234567";
                strkey[4] = i;
                token->insert(fstring(strkey, 8), &i);
            }
            for (uint32_t i = 0; i < 256; ++i) {
                char strkey[9] = "a1234567";
                strkey[2] = i;
                uint32_t val = i+256;
                token->insert(fstring(strkey, 8), &val);
            }
            {
                uint32_t val = UINT32_MAX;
                token->insert(fstring(""), &val);
                TERARK_VERIFY_EQ(token->value_of<uint32_t>(), UINT32_MAX);
            }
            token->release();
            Patricia::IteratorPtr iterp(trie.new_iter());
            Patricia::Iterator& iter = *iterp;
            printf("MainPatricia iter incr basic...\n");
            {
                TERARK_VERIFY(iter.seek_begin());
                TERARK_VERIFY(iter.value_of<uint32_t>() == UINT32_MAX);
                for (uint32_t i = 0; i < 512; ++i) {
                    TERARK_VERIFY(iter.incr());
                    TERARK_VERIFY(iter.value_of<uint32_t>() == i);
                }
                TERARK_VERIFY(!iter.incr());
            }
            printf("MainPatricia iter incr basic... passed\n");

            printf("MainPatricia iter decr basic...\n");
            {
                TERARK_VERIFY(iter.seek_end());
                uint32_t i = 512;
                while (i) {
                    --i;
                    TERARK_VERIFY_EQ(iter.value_of<uint32_t>(), i);
                    TERARK_VERIFY(iter.decr());
                }
                TERARK_VERIFY_EQ(iter.value_of<uint32_t>(), UINT32_MAX);
                TERARK_VERIFY(!iter.decr());
            }
            printf("MainPatricia iter decr basic... passed\n");
        }
        auto insert = [](MainPatricia& trie, const hash_strmap<>& strVec) {
            MainPatricia::WriterTokenPtr token(new MainPatricia::WriterToken());
            for (size_t i = 0, n = strVec.end_i(); i < n; i++) {
                token->acquire(&trie);
                fstring key = strVec.key(i);
                trie.insert(key, NULL, &*token);
                token->idle();
            }
            token->release();
        };
        unit_test_run<MainPatricia>(insert);
    }
}
/*
// for Automata<State>
template<class DFA>
void minimize_and_path_zip(DFA& dfa) {
    printf("dfa.adfa_minimize()...\n");
    dfa.adfa_minimize();
    printf("dfa.path_zip(\"DFS\")...\n");
    dfa.path_zip("DFS");
}
*/
template<class Trie, class DawgType>
void minimize_and_path_zip(NestTrieDAWG<Trie, DawgType>&) {}
void minimize_and_path_zip(MainPatricia&) {}

template<class DFA, class Inserter>
void unit_test_run(Inserter insert) {
    DFA dfa;
    test_empty_dfa(dfa);
    hash_strmap<> strVec;
    LineBuf line;
    size_t lineno = 0;
    printf("reading file %s ...\n", fname);
    while (line.getline(fp) > 0) {
        line.trim();
        strVec.insert_i(line);
        lineno++;
        if (lineno % TERARK_IF_DEBUG(1000, 10000) == 0) {
            //printf("lineno=%zd\n", lineno);
            printf("."); fflush(stdout);
        }
    }
    printf("\n");
    printf("done, lines=%zd...\n", lineno);
    if (strVec.size() > 0) {
        insert(dfa, strVec);
        minimize_and_path_zip(dfa);
        printf("strVec.sort_slow()...\n");
        strVec.sort_slow();
        printf("strVec.key(0).size() = %zd\n", strVec.key(0).size());
        test_run_impl(dfa, strVec);
    }
}

void test_run_impl(const MatchingDFA& dfa, const hash_strmap<>& strVec) {
    ADFA_LexIteratorUP iterU(dfa.adfa_make_iter(initial_state));
    auto iter = iterU.get();

    printf("test incr...\n");
    TERARK_VERIFY(iter->seek_begin());
    for (size_t i = 0; i + 1 < strVec.end_i(); ++i) {
        fstring iw = iter->word();
        fstring vw = strVec.key(i);
        if (iw != vw) {
            printf("iw:%zd: len=%zd: %.*s\n", i, iw.size(), iw.ilen(), iw.data());
            printf("vw:%zd: len=%zd: %.*s\n", i, vw.size(), vw.ilen(), vw.data());
        }
        TERARK_VERIFY(iw == vw);
        TERARK_VERIFY(iter->incr());
    }
    TERARK_VERIFY(!iter->incr());
    printf("test incr passed\n\n");

    printf("test decr...\n");
    TERARK_VERIFY(iter->seek_end() || strVec.empty());
    for(size_t i = strVec.end_i(); i-- > 1; ) {
        fstring iw = iter->word();
        fstring vw = strVec.key(i);
        if (iw != vw) {
            printf("iw:%zd: %.*s\n", i, iw.ilen(), iw.data());
            printf("vw:%zd: %.*s\n", i, vw.ilen(), vw.data());
        }
        TERARK_VERIFY(iw == vw);
        TERARK_VERIFY(iter->decr());
    }
    TERARK_VERIFY(!iter->decr());
    printf("test decr passed\n\n");

    printf("test lower_bound + incr...\n");
    valvec<size_t> shuf(strVec.size(), valvec_no_init());
    for (size_t i = 0; i < shuf.size(); ++i) shuf[i] = i;
    std::shuffle(shuf.begin(), shuf.end(), std::mt19937());
    size_t cnt = 10;
    for(size_t j = 0; j < strVec.end_i()/cnt; ++j) {
        size_t k = shuf[j];
        fstring key = strVec.key(k);
        TERARK_VERIFY_S(iter->seek_lower_bound(key), "%s", key);
        for (size_t i = k; i < std::min(k + cnt, strVec.end_i()-1); ++i) {
            fstring iw = iter->word();
            fstring vw = strVec.key(i);
            if (iw != vw) {
                printf("iw:%zd: len=%zd: %.*s\n", i, iw.size(), iw.ilen(), iw.data());
                printf("vw:%zd: len=%zd: %.*s\n", i, vw.size(), vw.ilen(), vw.data());
            }
            TERARK_VERIFY(iw == vw);
            TERARK_VERIFY(iter->incr());
        }
    }
    printf("test lower_bound + incr passed\n\n");

    printf("test lower_bound out of range key...\n");
    std::string key = strVec.key(strVec.size()-1).str();
    TERARK_VERIFY(iter->seek_lower_bound(key));
    TERARK_VERIFY(!iter->incr());
    for (size_t i = 0; i < key.size(); ++i) {
        std::string key2 = key;
        if ((unsigned char)key2[i] < 255) {
            key2[i]++;
            TERARK_VERIFY_S(!iter->seek_lower_bound(key2), "%s : %s", key, key2);
        }
    }
    key.push_back('1');
    TERARK_VERIFY_S(!iter->seek_lower_bound(key), "%s", key);
    printf("test lower_bound out of range key passed\n\n");

    printf("test lower_bound minimum key...\n");
    key = strVec.key(0).str();
    TERARK_VERIFY_S(iter->seek_lower_bound(key), "%s", key);
    TERARK_VERIFY_S_EQ(iter->word(), key);
    TERARK_VERIFY(!iter->decr());
    for (size_t i = 0; i < key.size(); ++i) {
        std::string key2 = key;
        if ((unsigned char)key2[i] > 0) {
            key2[i]--;
            TERARK_VERIFY_S(iter->seek_lower_bound(key2), "%s : %s", key, key2);
            TERARK_VERIFY_S_LT(key2, iter->word());
            TERARK_VERIFY(!iter->decr());
        }
    }
    if (key.size()) {
        std::string key2 = key;
        key2.pop_back();
        TERARK_VERIFY_S(iter->seek_lower_bound(key2), "%s : %s", key, key2);
        TERARK_VERIFY_S_LT(key2, iter->word());
        TERARK_VERIFY(!iter->decr());
    }
    printf("test lower_bound minimum key passed\n\n");

    printf("test lower_bound median key...\n");
    key = strVec.key(0).str();
    key.push_back('\1');
    TERARK_VERIFY(iter->seek_lower_bound(key) || strVec.size() == 1);
    printf("test lower_bound median key passed...\n\n");

    printf("test lower_bound + decr...\n");
    for(size_t j = 0; j < strVec.end_i()/cnt; ++j) {
        size_t k = shuf[j];
        k = std::max(k, cnt);
        fstring randKey = strVec.key(k);
        iter->seek_lower_bound(randKey);
        for (size_t i = k; i > k - cnt; ) {
            --i;
            TERARK_VERIFY(iter->decr());
            fstring iw = iter->word();
            fstring vw = strVec.key(i);
            if (iw != vw) {
                printf("--------------------------\n");
                printf("iw:%zd: %.*s\n", strVec.find_i(iw), iw.ilen(), iw.data());
                printf("vw:%zd: %.*s\n", i, vw.ilen(), vw.data());
            }
            TERARK_VERIFY(iw == vw);
        }
        for (size_t i = k - cnt; i < k; ++i) {
            fstring iw = iter->word();
            fstring vw = strVec.key(i);
            if (iw != vw) {
                printf("--------------------------\n");
                printf("iw:%zd: %.*s\n", strVec.find_i(iw), iw.ilen(), iw.data());
                printf("vw:%zd: %.*s\n", i, vw.ilen(), vw.data());
            }
            TERARK_VERIFY(iw == vw);
            TERARK_VERIFY(iter->incr());
        }
    }
    printf("test lower_bound + decr passed\n\n");

    printf("test lower_bound as upper_bound + decr...\n");
    for(size_t j = 0; j < strVec.end_i()/cnt; ++j) {
        size_t k = shuf[j];
        fstring rk = strVec.key(k);
        if (rk.size() == 0)
            continue;
        std::string randKey = rk.substr(0, rand() % rk.size())
                            + rk.substr(rand() % rk.size(), 1);
        k = strVec.lower_bound(randKey);
        if (k < cnt)
            continue; // did not meet test condition
        if (iter->seek_lower_bound(randKey)) {
            {
                fstring iw = iter->word();
                fstring vw = strVec.key(k);
                if (iw != vw) {
                    printf("RandKey: len=%zd: %s------------------\n", randKey.size(), randKey.c_str());
                    printf("iw:%zd: len=%zd: %.*s\n", k, iw.size(), iw.ilen(), iw.data());
                    printf("vw:%zd: len=%zd: %.*s\n", k, vw.size(), vw.ilen(), vw.data());
                }
                TERARK_VERIFY(iw == vw);
            }
            for (size_t i = k; i > k - cnt; ) {
                --i;
                TERARK_VERIFY(iter->decr());
                fstring iw = iter->word();
                fstring vw = strVec.key(i);
                if (iw != vw) {
                    printf("--------------------------\n");
                    printf("iw:%zd: %.*s\n", strVec.find_i(iw), iw.ilen(), iw.data());
                    printf("vw:%zd: %.*s\n", i, vw.ilen(), vw.data());
                }
                TERARK_VERIFY(iw == vw);
            }
            for (size_t i = k - cnt; i < k; ++i) {
                fstring iw = iter->word();
                fstring vw = strVec.key(i);
                if (iw != vw) {
                    printf("--------------------------\n");
                    printf("iw:%zd: %.*s\n", strVec.find_i(iw), iw.ilen(), iw.data());
                    printf("vw:%zd: %.*s\n", i, vw.ilen(), vw.data());
                }
                TERARK_VERIFY(iw == vw);
                TERARK_VERIFY(iter->incr());
            }
        }
        else {
            fstring maxKey = strVec.key(strVec.end_i()-1);
            TERARK_VERIFY_S_LT(maxKey, randKey);
        }
    }
    printf("test lower_bound as upper_bound + decr passed\n\n");

    printf("test seek_max_prefix\n");
    // TODO: Add more case
    for(size_t j = 0; j < strVec.end_i(); ++j) {
        size_t k = shuf[j];
        fstring rk = strVec.key(k);
        if (rk.size() == 0)
            continue;
        size_t r1 = rand() % rk.size();
        size_t r2 = rand() % rk.size();
        std::string randKey = rk.substr(0, r1) + rk.substr(r2, rk.size() - r2);
        size_t cplen = rk.commonPrefixLen(randKey);
        size_t partial_len  = iter->seek_max_prefix(randKey);
    //    printf("randKey = len=%zd: %s\n", randKey.size(), randKey.c_str());
    //    printf("cplen = %zd, partial_len = %zd, iter->word() = len=%zd: \"%s\"\n"
    //            , cplen, partial_len, iter->word().size(), iter->word().c_str());
        TERARK_VERIFY_LE(partial_len, randKey.size());
        TERARK_VERIFY_GE(partial_len, iter->word().size());
        TERARK_VERIFY_GE(partial_len, cplen);
        TERARK_VERIFY(memcmp(randKey.data(), iter->word().data(), iter->word().size()) == 0);
        k = strVec.lower_bound(randKey);
        TERARK_VERIFY(k == strVec.end_i() || iter->word() <= strVec.key(k));
        if (k < strVec.end_i() && strVec.key(k).startsWith(iter->word())) {
            TERARK_VERIFY(strVec.key(k).startsWith(iter->word()));
        }
        else if (k > 0) {
            TERARK_VERIFY(strVec.key(k-1).startsWith(iter->word()));
        }
        if (iter->incr()) {
            if (!(randKey < iter->word())) {
            //    printf("fail: randKey  = %s\n", randKey.c_str());
            //    printf("fail: iterWord = %s\n", iter->word().c_str());
            }
            //TERARK_VERIFY(randKey < iter->word());
        }
        iter->decr();
        if (iter->decr()) {
            //TERARK_VERIFY(randKey > iter->word());
        }
        iter->incr();
        iter->incr();
    }
    printf("test seek_max_prefix passed\n\n");
}


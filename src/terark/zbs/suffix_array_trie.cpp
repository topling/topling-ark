#include "suffix_array_trie.hpp"
#include <zstd/dictBuilder/divsufsort.h>
#include <terark/io/FileStream.hpp>
#include <terark/io/StreamBuffer.hpp>
#include <terark/io/DataIO.hpp>
#include <terark/util/profiling.hpp>

namespace terark {

void SuffixTrieCacheDFA::build_sa(SortableStrVec& strVec) {
	profiling pf;
	this->erase_all();
	strVec.make_ascending_offset();
	m_sa.resize_no_init(strVec.m_strpool.size());
	llong t0 = pf.now();
	divsufsort((byte*)strVec.m_strpool.data(), m_sa.data(), m_sa.size(), 0);
	llong t1 = pf.now();
	printf("divsufsort: %zd bytes, time: %f seconds, through-put: %f MB/s\n"
		, m_sa.size(), pf.sf(t0,t1), m_sa.size()/pf.uf(t0,t1)
		);
	m_strVec = &strVec;
//	m_strVec->m_strpool.resize(2 * m_sa.size(), '$'); // for test
//	bfs_build_cache(minFreq, maxDepth);
//	dfs_build_cache_loop(0, m_sa.size(), 0);
}

void SuffixTrieCacheDFA::dot_write_one_state(FILE* fp, size_t s, const char* ext_attr) const {
	long ls = s;
	long lo = this->states[s].suffixLow;
	long hi = this->states[s].suffixHig;
	if (v_is_pzip(ls)) {
		MatchContext ctx;
		fstring zs = v_get_zpath_data(ls, &ctx);
		char buf[1040];
		dot_escape(zs.data(), zs.size(), buf, sizeof(buf)-1);
		if (v_is_term(ls))
			fprintf(fp, "\tstate%ld[label=\"%ld(%ld %ld)\\n%s\" shape=\"doublecircle\" %s];\n", ls, ls, lo, hi, buf, ext_attr);
		else
			fprintf(fp, "\tstate%ld[label=\"%ld(%ld %ld)\\n%s\" %s];\n", ls, ls, lo, hi, buf, ext_attr);
	}
	else {
		if (v_is_term(ls))
			fprintf(fp, "\tstate%ld[label=\"%ld(%ld %ld)\" shape=\"doublecircle\" %s];\n", ls, ls, lo, hi, ext_attr);
		else
			fprintf(fp, "\tstate%ld[label=\"%ld(%ld %ld)\" %s];\n", ls, ls, lo, hi, ext_attr);
	}
}

struct SuffixTrieCacheDFA::BfsQueueElem {
	uint32_t depth; // matching depth, pos
	uint32_t state;
};
void
SuffixTrieCacheDFA::bfs_build_cache(size_t minTokenLen, size_t minFreq, size_t maxBfsDepth) {
	const byte_t* str = m_strVec->m_strpool.data();
	const int   * sa  = m_sa.data();
	const size_t  sa_size = m_sa.size();
	this->erase_all();
	this->states.erase_all();
	this->states.push_back();
	this->states[0].suffixLow = 0;
	this->states[0].suffixHig = sa_size;
	AutoFree<CharTarget<size_t> > children(this->sigma);
	valvec<BfsQueueElem> q1, q2;
	q1.push_back({0, 0});
	size_t bfsDepth = 0;
	while (!q1.empty() && bfsDepth < maxBfsDepth) {
		for(auto e : q1) {
			size_t lo = this->states[e.state].suffixLow;
			size_t hi = this->states[e.state].suffixHig;
			size_t depth = e.depth;
			if (sa[lo] + depth < sa_size) {
				size_t saLo = sa[lo];
				size_t saHi = sa[hi-1];
				if (str[saLo + depth] == str[saHi + depth]) {
					size_t maxPos = std::min(saLo + depth + state_t::MaxDepth, sa_size);
					do ++depth;
					while (	    saLo + depth < maxPos &&
							str[saLo + depth] == str[saHi + depth] );
					this->states[e.state].setZstrLen(depth - e.depth);
				//	this->states[e.state].set_pzip_bit();
				}
			}
			if (hi - lo < minFreq) {
				continue;
			}
			if (sa[lo] + depth >= sa_size) {
				lo++;
			}
			size_t childcnt = 0;
			while (lo < hi) {
				byte_t c = str[sa[lo] + depth];
				size_t u = sa_upper_bound(lo, hi, depth, c);
				size_t uppIdx = m_strVec->upper_bound_by_offset(sa[lo]);
				assert(uppIdx > 0);
				size_t endpos = m_strVec->m_index[uppIdx - 1].endpos();
				assert(endpos >= size_t(sa[lo]));
				if (sa[lo] + minTokenLen <= endpos) {
					size_t child = this->new_state();
					q2.push_back({uint32_t(depth) + 1, uint32_t(child)});
					children[childcnt].ch = c;
					children[childcnt].target = child;
					this->states[child].suffixLow = lo;
					this->states[child].suffixHig = u;
					childcnt++;
				}
				lo = u;
			}
			this->add_all_move(e.state, children, childcnt);
		//	int keepStackFrameForMSVC = 1;
		}
		q1.swap(q2);
		q2.erase_all();
		bfsDepth++;
	}
//	this->write_dot_file("suffix-array.dot");
//	sa_print_stat();
}

void
SuffixTrieCacheDFA::pfs_build_cache(size_t minTokenLen, size_t minFreq) {
	const byte_t* str = m_strVec->m_strpool.data();
	const int   * sa  = m_sa.data();
	const size_t  sa_size = m_sa.size();
	this->erase_all();
	this->states.erase_all();
	this->states.push_back();
	this->states[0].suffixLow = 0;
	this->states[0].suffixHig = sa_size;
	AutoFree<CharTarget<size_t> > children(this->sigma);
	valvec<BfsQueueElem> stack;
	stack.push_back({0, 0});
	while (!stack.empty()) {
		auto e = stack.pop_val();
		size_t lo = this->states[e.state].suffixLow;
		size_t hi = this->states[e.state].suffixHig;
		size_t depth = e.depth;
		if (sa[lo] + depth < sa_size) {
			size_t saLo = sa[lo];
			size_t saHi = sa[hi-1];
			if (str[saLo + depth] == str[saHi + depth]) {
				size_t maxPos = std::min(saLo + depth + state_t::MaxDepth, sa_size);
				do ++depth;
				while (	    saLo + depth < maxPos &&
						str[saLo + depth] == str[saHi + depth] );
				this->states[e.state].setZstrLen(depth - e.depth);
			//	this->states[e.state].set_pzip_bit();
			}
		}
		if (hi - lo < minFreq) {
			continue;
		}
		if (sa[lo] + depth >= sa_size) {
			lo++;
		}
		size_t childcnt = 0;
		while (lo + minFreq <= hi) {
			byte_t c = str[sa[lo] + depth];
			size_t u = sa_upper_bound(lo, hi, depth, c);
			size_t uppIdx = m_strVec->upper_bound_by_offset(sa[lo]);
			assert(uppIdx > 0);
			size_t endpos = m_strVec->m_index[uppIdx - 1].endpos();
			assert(endpos >= size_t(sa[lo]));
			if (sa[lo] + minTokenLen <= endpos) {
				size_t child = this->new_state();
				stack.push_back({uint32_t(depth + 1), uint32_t(child)});
				children[childcnt].ch = c;
				children[childcnt].target = child;
				this->states[child].suffixLow = lo;
				this->states[child].suffixHig = u;
				childcnt++;
			}
			lo = u;
		}
		this->add_all_move(e.state, children, childcnt);
	//	int keepStackFrameForMSVC = 1;
	}
//	this->write_dot_file("suffix-array.dot");
//	sa_print_stat();
}

struct SuffixTrieCacheDFA::DfsBuildLoopParam {
	size_t minTokenLen;
	size_t minFreq;
	size_t maxDepth;
	valvec<CharTarget<size_t> > children;
};
void
SuffixTrieCacheDFA::dfs_build_cache(size_t minTokenLen, size_t minFreq, size_t maxDepth) {
	this->erase_all();
	this->states.erase_all();
	this->states.push_back();
	this->states[0].suffixLow = 0;
	this->states[0].suffixHig = m_sa.size();
	DfsBuildLoopParam param;
	param.minTokenLen = minTokenLen;
	param.minFreq  = minFreq;
	param.maxDepth = maxDepth;
	dfs_build_cache_loop(0, m_sa.size(), 0, initial_state, param);
}
void
SuffixTrieCacheDFA::dfs_build_cache_loop(size_t lo, size_t hi,
										 size_t depth, size_t state,
										 DfsBuildLoopParam& param) {
	if (depth > param.maxDepth) {
		return;
	}
	if (m_sa[lo] + depth >= m_sa.size()) {
		lo++;
	}
	const byte_t* str = m_strVec->m_strpool.data();
	while (lo < hi) {
		byte_t c = str[m_sa[lo] + depth];
		size_t u = sa_upper_bound(lo, hi, depth, c);
	//	size_t freq = u - lo;
	//	double score = depth * log(depth) * freq;
	//	printf("lo = %zd, u = %zd, lopos = %zd, depth = %zd, score = %f, term = %.*s\n",
	//			lo, u, lopos, depth, score, int(depth), str + lopos);
		if (u - lo >= param.minFreq) {
			size_t uppIdx = m_strVec->upper_bound_by_offset(m_sa[lo]);
			assert(uppIdx > 0);
			size_t endpos = m_strVec->m_index[uppIdx - 1].endpos();
			assert(endpos >= size_t(m_sa[lo]));
			if (m_sa[lo] + param.minTokenLen <= endpos) {
				size_t child = this->new_state();
				this->states[child].suffixLow = lo;
				this->states[child].suffixHig = u;
				this->states[child].setZstrLen(depth + 1);
				this->add_move_checked(state, child, c);
				dfs_build_cache_loop(lo, u, depth + 1, child, param);
			}
		}
		lo = u;
	}
}

bool
SuffixTrieCacheDFA::sa_state_move(const MatchStatus& curr, byte_t ch, MatchStatus* next)
const {
	assert(&curr != next);
	if (curr.state < this->states.size()) {
		next->state = this->state_move(curr.state, ch);
		if (next->state < this->states.size()) {
			next->depth = curr.depth + 1;
			next->lo = this->states[next->state].suffixLow;
			next->hi = this->states[next->state].suffixHig;
			assert(m_strVec->m_strpool[m_sa[next->lo] + curr.depth] == ch);
			return true;
		}
	}
	auto rng = sa_equal_range(curr.lo, curr.hi, curr.depth, ch);
	if (rng.first < rng.second) {
		next->lo = rng.first;
		next->hi = rng.second;
		next->depth = curr.depth + 1;
		next->state = this->nil_state;
		assert(m_strVec->m_strpool[m_sa[next->lo] + curr.depth] == ch);
		return true;
	}
	return false;
}

// after freq narrowed to minFreq, see max cover
SuffixTrieCacheDFA::MatchStatus
SuffixTrieCacheDFA::sa_match_max_cover(fstring str, size_t minLen, size_t minFreq)
const {
	const byte_t* pool = m_strVec->m_strpool.data();
	const int* sa = m_sa.data();
	MatchStatus curr = sa_root();
	while (curr.depth < str.size()) {
		size_t zlen = this->states[curr.state].getZstrLen();
		size_t zend = std::min(str.size(), curr.depth + zlen);
		auto   zptr = pool + sa[curr.lo];
		for (; curr.depth < zend; curr.depth++) {
			if (zptr[curr.depth] != str.uch(curr.depth))
				goto TryNextState;
		}
		if (str.size() == curr.depth) {
			return curr;
		}
	TryNextState:
		byte_t ch = str.uch(curr.depth);
		size_t child = this->state_move(curr.state, ch);
		if (this->nil_state != child) {
			curr.state = child;
			curr.lo = this->states[curr.state].suffixLow;
			curr.hi = this->states[curr.state].suffixHig;
			assert(curr.freq() >= minFreq); // cache should has a bigger freq
			assert(pool[sa[curr.lo] + curr.depth] == ch);
			curr.depth++;
		}
		else
			goto SearchSA;
	}
	return curr;
SearchSA:
	assert(curr.freq() >= minFreq); // cache should has a bigger freq
	curr.state = this->nil_state;
	size_t  saLen = m_sa.size();
	while (curr.depth < str.size()) {
		{
			size_t saLo = sa[curr.lo+0];
			size_t saHi = sa[curr.hi-1];
			while (      saLo + curr.depth < saLen &&
					pool[saLo + curr.depth] == str.uch(curr.depth) &&
					pool[saHi + curr.depth] == str.uch(curr.depth) )
			{
				if (++curr.depth == str.size())
					return curr;
			}
			assert(saLo + curr.depth <= saLen);
		}
		byte_t ch = str[curr.depth];
		auto rng = sa_equal_range(curr.lo, curr.hi, curr.depth, ch);
		if (rng.second - rng.first >= minFreq &&
			(curr.depth < minLen ||
				curr.depth * curr.freq() < (curr.depth+1)*(rng.second - rng.first)))
		{
			curr.lo = rng.first;
			curr.hi = rng.second;
			curr.depth++;
		} else
			break;
	}
	return curr;
}

inline double xlogx(double x) { return x * log(x); }
inline double sqr(double x) { return x * x; }

static inline double reduced(double len, double total, double freq) {
	double avgPtrBytes = log2(total/len)/8;
	return freq * len - (freq * avgPtrBytes + len);
}

// after freq narrowed to minFreq, see max cover
SuffixTrieCacheDFA::MatchStatus
SuffixTrieCacheDFA::sa_match_max_score(fstring str, size_t minLen, size_t minFreq)
const {
	const byte_t* pool = m_strVec->m_strpool.data();
	const int* sa = m_sa.data();
	MatchStatus curr = sa_root();
	while (curr.depth < str.size()) {
		size_t zlen = this->states[curr.state].getZstrLen();
		size_t zend = std::min(str.size(), curr.depth + zlen);
		auto   zptr = pool + sa[curr.lo];
		for (; curr.depth < zend; curr.depth++) {
			if (zptr[curr.depth] != str.uch(curr.depth))
				goto TryNextState;
		}
		if (str.size() == curr.depth) {
			return curr;
		}
	TryNextState:
		byte_t ch = str.uch(curr.depth);
		size_t child = this->state_move(curr.state, ch);
		if (this->nil_state != child) {
			curr.state = child;
			curr.lo = this->states[curr.state].suffixLow;
			curr.hi = this->states[curr.state].suffixHig;
			assert(curr.freq() >= minFreq); // cache should has a bigger freq
			assert(pool[sa[curr.lo] + curr.depth] == ch);
			curr.depth++;
		}
		else
			goto SearchSA;
	}
	return curr;
SearchSA:
	assert(curr.freq() >= minFreq); // cache should has a bigger freq
	curr.state = this->nil_state;
	size_t  saLen = m_sa.size();
	while (curr.depth < str.size()) {
		{
			size_t saLo = sa[curr.lo+0];
			size_t saHi = sa[curr.hi-1];
			while (      saLo + curr.depth < saLen &&
					pool[saLo + curr.depth] == str.uch(curr.depth) &&
					pool[saHi + curr.depth] == str.uch(curr.depth) )
			{
				if (++curr.depth == str.size())
					return curr;
			}
			assert(saLo + curr.depth <= saLen);
		}
		byte_t ch = str[curr.depth];
		auto rng = sa_equal_range(curr.lo, curr.hi, curr.depth, ch);
		size_t freq2 = rng.second - rng.first;
		if (freq2 >= minFreq &&
			(curr.depth < minLen ||
				reduced(curr.depth+0, saLen, curr.freq()) <
				reduced(curr.depth+1, saLen, freq2)))
		{
			curr.lo = rng.first;
			curr.hi = rng.second;
			curr.depth++;
		} else
			break;
	}
	return curr;
}

SuffixTrieCacheDFA::MatchStatus
SuffixTrieCacheDFA::sa_match_max_length(fstring str, size_t minFreq) const {
	const byte_t* pool = m_strVec->m_strpool.data();
	const int* sa = m_sa.data();
	MatchStatus curr = sa_root();
	while (curr.depth < str.size()) {
		size_t zlen = this->states[curr.state].getZstrLen();
		size_t zend = std::min(str.size(), curr.depth + zlen);
		auto   zptr = pool + sa[curr.lo];
		for (; curr.depth < zend; curr.depth++) {
			if (zptr[curr.depth] != str.uch(curr.depth))
				goto TryNextState;
		}
		if (str.size() == curr.depth) {
			return curr;
		}
	TryNextState:
		byte_t ch = str.uch(curr.depth);
		size_t child = this->state_move(curr.state, ch);
		if (this->nil_state != child) {
			curr.state = child;
			curr.lo = this->states[curr.state].suffixLow;
			curr.hi = this->states[curr.state].suffixHig;
			assert(curr.freq() >= minFreq); // cache should has a bigger freq
			assert(pool[sa[curr.lo] + curr.depth] == ch);
			curr.depth++;
		}
		else
			goto SearchSA;
	}
	return curr;
SearchSA:
	assert(curr.freq() >= minFreq); // cache should has a bigger freq
	curr.state = this->nil_state;
	size_t  saLen = m_sa.size();
	while (curr.depth < str.size()) {
		{
			size_t saLo = sa[curr.lo+0];
			size_t saHi = sa[curr.hi-1];
			while (      saLo + curr.depth < saLen &&
					pool[saLo + curr.depth] == str.uch(curr.depth) &&
					pool[saHi + curr.depth] == str.uch(curr.depth) )
			{
				if (++curr.depth == str.size())
					return curr;
			}
			assert(saLo + curr.depth <= saLen);
		}
		byte_t ch = str[curr.depth];
		auto rng = sa_equal_range(curr.lo, curr.hi, curr.depth, ch);
		if (rng.second - rng.first >= minFreq) {
			curr.lo = rng.first;
			curr.hi = rng.second;
			curr.depth++;
		} else
			break;
	}
	return curr;
}

SuffixTrieCacheDFA::MatchStatus
SuffixTrieCacheDFA::sa_match_max_length_slow(fstring str, size_t minFreq)
const {
	MatchStatus curr = sa_root();
	MatchStatus next;
	for(size_t i = 0; i < str.size(); ++i) {
		byte_t c = str[i];
		if (sa_state_move(curr, c, &next)) {
			assert(m_strVec->m_strpool[m_sa[next.lo] + curr.depth] == c);
			if (next.freq() >= minFreq)
				curr = next;
			else
				return curr;
		}
		else
			return curr;;
	}
	return curr;
}

size_t SuffixTrieCacheDFA::make_lz_trie(fstring fpath, size_t minLen) const {
	const int* sa = m_sa.data();
	FileStream fp;
	NativeDataOutput<OutputBuffer> dio;
	dio.attach(&fp);
	fp.open(fpath.c_str(), "wb");
	fp.disbuf();
	size_t numToken = 0;
	for (size_t i = 0; i < m_strVec->size(); ++i) {
		fstring str = (*m_strVec)[i];
		for(size_t j = 0; j < str.size(); ) {
			auto res = this->sa_match_max_length(str.substr(j), 2);
			size_t len = std::min(std::max(res.depth, minLen), str.size()-j);
			dio << uint32_t(sa[res.lo]);
			dio << uint32_t(len);
			j += len;
			numToken++;
		}
	}
	return numToken;
}

size_t
SuffixTrieCacheDFA::sa_lower_bound(size_t lo, size_t hi, size_t depth, byte_t ch)
const {
	assert(lo < hi);
	assert(hi <= m_sa.size());
	const int* sa = m_sa.data();
	const byte_t* strpool = m_strVec->m_strpool.data();
	assert(sa[lo] + depth <= m_sa.size());
	if (terark_unlikely(sa[lo] + depth >= m_sa.size())) {
		lo++;
		assert(sa[lo] + depth < m_sa.size());
	}
	while (lo < hi) {
		size_t mid = (lo + hi) / 2;
		byte_t hitChr = strpool[sa[mid] + depth];
		if (hitChr < ch) // lower bound
			lo = mid + 1;
		else
			hi = mid;
	}
	return lo;
}

size_t
SuffixTrieCacheDFA::sa_upper_bound(size_t lo, size_t hi, size_t depth, byte_t ch)
const {
	assert(lo < hi);
	assert(hi <= m_sa.size());
	const int* sa = m_sa.data();
	const byte_t* strpool = m_strVec->m_strpool.data();
	assert(sa[lo] + depth <= m_sa.size());
	if (terark_unlikely(sa[lo] + depth >= m_sa.size())) {
		lo++;
		assert(sa[lo] + depth < m_sa.size());
	}
	while (lo < hi) {
		size_t mid = (lo + hi) / 2;
		byte_t hitChr = strpool[sa[mid] + depth];
		if (hitChr <= ch) // upper bound
			lo = mid + 1;
		else
			hi = mid;
	}
	return lo;
}

std::pair<size_t, size_t>
SuffixTrieCacheDFA::sa_equal_range(size_t lo, size_t hi, size_t depth, byte_t ch)
const {
	assert(lo < hi);
	assert(hi <= m_sa.size());
	const int* sa = m_sa.data();
	const byte_t* strpool = m_strVec->m_strpool.data();
	assert(sa[lo] + depth <= m_sa.size());
	if (terark_unlikely(sa[lo] + depth >= m_sa.size())) {
		lo++;
		assert(sa[lo] + depth < m_sa.size());
	}
	while (lo < hi) {
		assert(sa[lo] + depth < m_sa.size());
		size_t mid = (lo + hi) / 2;
		assert(sa[mid] + depth < m_sa.size());
		byte_t hitChr = strpool[sa[mid] + depth];
		if (hitChr < ch)
			lo = mid + 1;
		else if (hitChr > ch)
			hi = mid;
		else
			goto Found;
	}
	return std::make_pair(lo, lo);
Found:
	size_t lo1 = lo, hi1 = hi;
	while (lo1 < hi1) {
		size_t mid = (lo1 + hi1) / 2;
		byte_t hitChr = strpool[sa[mid] + depth];
		if (hitChr < ch) // lower bound
			lo1 = mid + 1;
		else
			hi1 = mid;
	}
	size_t lo2 = lo, hi2 = hi;
	while (lo2 < hi2) {
		size_t mid = (lo2 + hi2) / 2;
		byte_t hitChr = strpool[sa[mid] + depth];
		if (hitChr <= ch) // upper bound
			lo2 = mid + 1;
		else
			hi2 = mid;
	}
	return std::make_pair(lo1, lo2);
}

void SuffixTrieCacheDFA::sa_print_match(const MatchStatus& match) {
	const byte_t* strpool = m_strVec->m_strpool.data();
	for(size_t i = match.lo; i < match.hi; ++i) {
		size_t sufPos = m_sa[i];
		size_t recIdx = m_strVec->upper_bound_by_offset(sufPos) - 1;
		assert(recIdx < m_strVec->size());
		size_t recBeg = m_strVec->m_index[recIdx].offset;
		size_t recEnd = m_strVec->m_index[recIdx].endpos();
		size_t recLen = m_strVec->m_index[recIdx].length;
		assert(sufPos >= recBeg);
		assert(sufPos <  recEnd);
		byte_t const* recData = strpool + recBeg;
		printf("rec[id=%04zd off=%zd len=%zd], sufPos=%zd match.depth=%zd\n"
			, recIdx, recBeg, recLen, sufPos, match.depth
			);
	//	printf("\t%.*s\n", int(recLen), recData);
		printf("\t%.*s\33[31;1m%.*s\33[0m%.*s\n"
			, int(sufPos - recBeg), recData
			, int(match.depth), recData + (sufPos - recBeg)
			, int(recEnd - (sufPos + match.depth)), strpool + (sufPos + match.depth)
			);
	}
}

void SuffixTrieCacheDFA::sa_print_stat() const {
	printf("total string length: %10zd\n", m_strVec->str_size());
	printf("total string number: %10zd\n", m_strVec->size());
	printf("total cache states : %10zd\n", this->states.size());
	printf("total free  states : %10zd\n", this->num_free_states());
	printf("total cache memsize: %10zd, avg size per state: %f\n"
		, this->mem_size(), this->mem_size() / (this->states.size()+.0));
	printf("SuffixArray memsize: %10zd\n", m_sa.used_mem_size());
}


} // namespace terark


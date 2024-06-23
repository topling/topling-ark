class TrieMapState {
public:
    TrieMapState() { init(); }

enum MyEnumConst {
	MemPool_Align = sizeof(uint32_t),
	free_flag = (1 << 4) - 1,
	PtrBits = 28,
	sigma = 256,
};

uint32_t  spos  : PtrBits; // spos: saved position
uint32_t  nbm   :  4; // cnt of uint32 for bitmap, in [0, 9)
uint32_t  minch :  8;
uint32_t  mapid : 24;

public:
typedef uint32_t  state_id_t;
typedef uint32_t  position_t;
static const uint32_t max_mapid = uint32_t((1UL << 24) - 2);
static const uint32_t nil_mapid = uint32_t((1UL << 24) - 1);
static const uint32_t nil_state = uint32_t((1UL << PtrBits) - 1);
static const uint32_t max_pos = uint32_t(nil_state) * MemPool_Align;

// max_state <= nil_state, and could be overridden
static const uint32_t max_state = nil_state - 1;

void init() {
	spos = nil_state;
	nbm = 0;
	minch = 0;
	mapid = nil_mapid;
}

void setch(auchar_t ch) {
	minch = ch;
	nbm = 0;
}
void setMinch(auchar_t ch) {
	assert(ch < minch);
	auchar_t maxch = (0 == nbm) ? minch : getMaxch();
	nbm = (maxch - ch) / 32 + 1;
	minch = ch;
}
void setMaxch(auchar_t ch) {
	assert(ch > minch);
	assert(ch > getMaxch());
	nbm = (ch - minch) / 32 + 1;
}
auchar_t getMaxch() const {
	if (0 == nbm)
	   return minch;
	unsigned maxch = minch + (nbm*32 - 1);
	return std::min<int>(sigma-1, maxch);
}
auchar_t getMinch() const { return minch; }
bool range_covers(auchar_t ch) const { return ch == minch || (ch > minch && ch < minch + nbm*32); }
int  rlen() const { assert(0 != nbm); return nbm * 32; }
bool has_children() const { return 0 != nbm || spos != nil_state; }
bool more_than_one_child() const { return 0 != nbm; }

bool is_term() const { return nil_mapid != mapid; }
bool is_pzip() const { return false; }
bool is_free() const { return free_flag == nbm; }
void set_term_bit() { assert(0); }
void set_pzip_bit() { assert(0); }
void clear_term_bit() { assert(0); }
void set_free() { assert(0); }
void setpos(size_t tpos) {
	assert(tpos % MemPool_Align == 0);
	if (tpos >= max_pos) {
		string_appender<> msg;
		msg << BOOST_CURRENT_FUNCTION << ": too large tpos=" << tpos
			<< ", limit=" << max_pos;
		throw std::out_of_range(msg);
	}
	spos = position_t(tpos / MemPool_Align);
}
size_t getpos() const { return size_t(spos) * MemPool_Align; }
state_id_t get_target() const { return spos; }
void set_target(state_id_t t) { spos = t; }

typedef state_id_t transition_t;
static transition_t first_trans(state_id_t t) { return t; }

}; // end class PackedState



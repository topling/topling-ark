#define ClassName TERARK_PP_Identity(TERARK_PP_Arg0 ClassName_StateID_PtrBits_nbmBits_Sigma)
#define StateID   TERARK_PP_Identity(TERARK_PP_Arg1 ClassName_StateID_PtrBits_nbmBits_Sigma)
#define PtrBits   TERARK_PP_Identity(TERARK_PP_Arg2 ClassName_StateID_PtrBits_nbmBits_Sigma)
#define nbmBits   TERARK_PP_Identity(TERARK_PP_Arg3 ClassName_StateID_PtrBits_nbmBits_Sigma)
#define Sigma     TERARK_PP_Identity(TERARK_PP_Arg4 ClassName_StateID_PtrBits_nbmBits_Sigma)

// When MyBytes is 6, intentional waste a bit: sbit is 32, nbm is 5
// When sbits==34, sizeof(PackedState)==6, StateID is uint64, the State34
class ClassName {
public:
    ClassName() { init(); }

BOOST_STATIC_ASSERT(Sigma >= 256);
BOOST_STATIC_ASSERT(Sigma <= 512);

enum MyEnumConst {
	MemPool_Align = sizeof(StateID),
	char_bits = StaticUintBits<Sigma-1>::value,
	free_flag = (1 << nbmBits) - 1,
	sigma = Sigma,
};

#ifdef _MSC_VER
	typedef boost::mpl::if_c
	< (PtrBits + nbmBits + 2 + char_bits <= 32)
	, StateID
	, boost::mpl::if_c
	  < (Sigma <= 256)
	  , unsigned char
	  , unsigned short
	  >::type
	>::type	CharUint;
	typedef boost::mpl::if_c
	< PtrBits == 32, CharUint, StateID >::type FlagUint;
#else
	typedef auchar_t CharUint;
	typedef auchar_t FlagUint;
#endif

StateID  spos     : PtrBits; // spos: saved position
FlagUint nbm      : nbmBits; // cnt of uint32 for bitmap, in [0, 9)
FlagUint term_bit : 1;
FlagUint pzip_bit : 1; // is path compressed(zipped)?
CharUint minch    : char_bits;

public:
typedef StateID  state_id_t;
typedef StateID  position_t;
static const StateID nil_state = state_id_t((uint64_t(1) << PtrBits) - 1);
static const uint64_t max_pos1 = uint64_t(nil_state) * MemPool_Align;
static const size_t max_pos = size_t(max_pos1 <= size_t(-1) ? max_pos1 : nil_state);

// max_state <= nil_state, and could be overridden
static const StateID max_state = nil_state - 1;

void init() {
	spos = nil_state;
	nbm = 0;
	term_bit = 0;
	pzip_bit = 0;
	minch = 0;
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
	return std::min<int>(Sigma-1, maxch);
}
auchar_t getMinch() const { return minch; }
bool range_covers(auchar_t ch) const { return ch == minch || (ch > minch && ch < minch + nbm*32); }
int  rlen() const { assert(0 != nbm); return nbm * 32; }
bool has_children() const { return 0 != nbm || spos != nil_state; }
bool more_than_one_child() const { return 0 != nbm; }

bool is_term() const { return term_bit; }
bool is_pzip() const { return pzip_bit; }
bool is_free() const { return free_flag == nbm; }
void set_term_bit() { term_bit = 1; }
void set_pzip_bit() { pzip_bit = 1; }
void clear_term_bit() { term_bit = 0; }
void set_free() { nbm = free_flag; }
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

	BOOST_STATIC_ASSERT((PtrBits + nbmBits + 2 + char_bits) % 8 == 0);
}; // end class PackedState

#undef ClassName_StateID_PtrBits_nbmBits_Sigma

#undef ClassName
#undef StateID
#undef PtrBits
#undef nbmBits
#undef Sigma


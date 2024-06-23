
	BOOST_STATIC_ASSERT(Sigma <= LargeSigma);
	enum { char_bits = StaticUintBits<Sigma-1>::value };
public:
	typedef typename boost::mpl::if_c
			< StateBytes <= 5
			, uint32_t
			, uint64_t
			>
			::type  state_id_t
			;
	typedef typename boost::mpl::if_c
			<(StateBytes % 4 == 0 || Sigma > 256)
			, state_id_t
			, unsigned char
			>
			::type  char_uint_t
			;
	typedef typename boost::mpl::if_c
			< char_bits + 3 <= 8
			, char_uint_t
			, state_id_t
			>
			::type  flag_uint_t
			;

	enum { state_id_bits = StateBytes*8 - 3 - char_bits };
public:
	enum { sigma = Sigma };
	#pragma pack(push,1)
	struct State {
		union u_ {
			struct state_ {
				state_id_t  dest : state_id_bits;
				flag_uint_t pzip_bit : 1;
				flag_uint_t term_bit : 1;
				flag_uint_t last_bit : 1; // is last edge of a state?
				char_uint_t       ch : char_bits;
			} s;
			struct trans_ {
				state_id_t  dest : state_id_bits;
				flag_uint_t bn64 : 3;
				char_uint_t   ch : char_bits;
			} t;
			struct zpath_ {
				unsigned char data[StateBytes];
			} z;
		} u;
		State() {
			memset(this, 0, sizeof(*this));
			u.s.dest = nil_state;
		}
	};
	#pragma pack(pop)
	BOOST_STATIC_ASSERT(sizeof(State) == StateBytes);

	typedef State state_t;
	typedef state_id_t transition_t;
	static const state_id_t max_state = state_id_t((uint64_t(1) << state_id_bits) - 2);
	static const state_id_t nil_state = state_id_t((uint64_t(1) << state_id_bits) - 1);

	MyType() {
		m_dyn_sigma = sigma;
		m_gnode_states = 0;
		m_transition_num = 0;
	}

	void clear() {
		states.clear();
		m_zpath_states = 0;
		m_total_zpath_len = 0;
	}
	void erase_all() {
		states.erase_all();
		m_zpath_states = 0;
		m_total_zpath_len = 0;
	}
	void swap(MyType& y) {
		BaseDFA::risk_swap(y);
		states.swap(y.states);
	}

	bool has_freelist() const override final { return false; }

	size_t total_states() const { return states.size(); }
	size_t mem_size() const override final { return sizeof(State) * states.size(); }
	size_t num_used_states() const { return states.size(); }
	size_t num_free_states() const { return 0; }
	size_t v_gnode_states()  const override final { return m_gnode_states; }

	template<class Au>
	void build_from(const char* WalkerName, const Au& au, typename Au::state_id_t root = initial_state) {
		if (strcasecmp(WalkerName, "BFS") == 0) {
			build_from_aux<BFS_GraphWalker<typename Au::state_id_t> >(au, root);
			return;
		}
		if (strcasecmp(WalkerName, "DFS") == 0) {
			build_from_aux<DFS_GraphWalker<typename Au::state_id_t> >(au, root);
			return;
		}
		if (strcasecmp(WalkerName, "PFS") == 0) {
			build_from_aux<PFS_GraphWalker<typename Au::state_id_t> >(au, root);
			return;
		}
		THROW_STD(invalid_argument, "unknown WalkerName=%s", WalkerName);
	}

	size_t v_num_children(size_t curr) const override final
	{ return this->num_children(curr); }

	bool v_has_children(size_t s) const override final {
		return !(states[s].u.s.last_bit && nil_state == states[s].u.s.dest);
	}

	bool is_pzip(size_t s) const {
		assert(s < states.size());
		return states[s].u.s.pzip_bit;
	}
	bool is_term(size_t s) const {
		assert(s < states.size());
		return states[s].u.s.term_bit;
	}
	bool is_free(size_t s) const {
		assert(s < states.size());
		return false;
	}
private:
	static size_t slots_of_bytes(size_t bytes) {
		return (bytes + sizeof(State)-1) / sizeof(State);
	}

public:
	~MyType() {
		if (this->mmap_base) {
			states.risk_release_ownership();
		}
	}

	size_t total_transitions() const { return m_transition_num; }
	void set_trans_num(size_t tn) { this->m_transition_num = tn; }

protected:
	valvec<State> states;
	size_t m_gnode_states;
	size_t m_transition_num;

#include "for_each_suffix.hpp"
#include "match_key.hpp"
#include "match_path.hpp"
#include "match_prefix.hpp"
#include "accept.hpp"
#include "dfa_const_virtuals.hpp"
//#include "post_order.hpp"
#include "flat_dfa_mmap.hpp"
#include "flat_dfa_data_io.hpp"
#include "state_move_fast.hpp"
#include "adfa_iter.hpp"
#include "for_each_word.hpp"
#include "match_pinyin.hpp"



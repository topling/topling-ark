public:
typedef MatchingDFA::OnMove OnMove;
typedef MatchingDFA::OnDest OnDest;
typedef MatchingDFA::OnNthWord OnNthWord;
typedef MatchingDFA::OnMatchKey OnMatchKey;
typedef MatchingDFA::OnNthWordEx OnNthWordEx;

typedef typename super::state_id_t state_id_t;
typedef typename super::transition_t transition_t;

using super::sigma;
using super::nil_state;
using super::max_state;

using super::total_states;

using super::is_free;
using super::is_pzip;
using super::is_term;

using super::set_term_bit;
//using super::set_pzip_bit;

//using super::has_children;
//using super::num_children;
using super::state_move;
using super::for_each_move;
using super::for_each_dest;
using super::for_each_dest_rev;
using super::add_all_move;
using super::del_all_move;
using super::total_transitions;
using super::internal_get_states;

using super::get_zpath_data;
using super::new_state;
using super::num_free_states;
using super::num_used_states;
using super::num_zpath_states;

using super::get_sigma;

using super::m_is_dag;
using super::m_kv_delim;

using super::m_atom_dfa_num;
using super::m_dfa_cluster_num;
using super::m_total_zpath_len;
using super::m_zpath_states;

template<class DataIO>
friend void DataIO_loadObject(DataIO& dio, MyType& x) {
	dio >> static_cast<super&>(x);
}
template<class DataIO>
friend void DataIO_saveObject(DataIO& dio, const MyType& x) {
	dio << static_cast<const super&>(x);
}

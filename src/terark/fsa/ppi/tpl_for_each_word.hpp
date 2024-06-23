/// @{
/// @param on_nth_word(size_t nth, fstring word, state_id_t accept_state)
///    calling of on_nth_word is in lexical_graphical order,
///    nth is the ordinal, 0 based
/// @returns  number of all words
template<class OnNthWordT>
size_t tpl_for_each_word(OnNthWordT on_match) const {
	return tpl_for_each_word<OnNthWordT>(initial_state, 0, on_match);
}
template<class OnNthWordT>
size_t tpl_for_each_word(OnNthWordT* on_match) const {
	return tpl_for_each_word<OnNthWordT&>(initial_state, 0, *on_match);
}
template<class OnNthWordT>
size_t tpl_for_each_word(size_t root, size_t zidx, OnNthWordT* on_match) const {
	return tpl_for_each_word<OnNthWordT&>(root, zidx, *on_match);
}
template<class OnNthWordT>
size_t tpl_for_each_word(size_t root, size_t zidx, OnNthWordT on_match) const {
	MatchContext ctx;
	NonRecursiveForEachWord forEachWord(&ctx);
	return forEachWord(*this, root, zidx, &on_match);
}
/*
size_t for_each_word(size_t root, size_t zidx, const OnNthWord& on_word)
const override final {
assert(m_is_dag);
check_is_dag(BOOST_CURRENT_FUNCTION);
return tpl_for_each_word(root, zidx, ForEachOnNthWord<>(on_word));
}
*/
///@}



public:
///@param WalkMethod "bfs" or "dfs"
void normalize(const char* WalkMethod) {
	MyType res;
	res.normalize(*this, WalkMethod);
	this->swap(res);
}

void path_zip(const char* WalkMethod, size_t min_zstr_len = 2) {
	MyType zipped;
	zipped.path_zip(*this, WalkMethod, min_zstr_len);
	this->swap(zipped);
}

void remove_dead_states() {
	MyType tmp;
	tmp.remove_dead_states(*this);
	this->swap(tmp);
}

void graph_dfa_minimize() {
	MyType minimized;
	this->graph_dfa_minimize(minimized);
	this->swap(minimized);
}
void trie_dfa_minimize() {
	MyType minimized;
	this->trie_dfa_minimize(minimized);
	this->swap(minimized);
}
void adfa_minimize() {
	MyType minimized;
	this->adfa_minimize(minimized);
	this->swap(minimized);
}



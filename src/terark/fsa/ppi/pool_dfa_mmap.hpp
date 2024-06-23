protected:
void finish_load_mmap(const DFA_MmapHeader* base) override {
	assert(sizeof(State) == base->state_size);
	byte_t* bbase = (byte_t*)base;

	this->states.clear();
	this->pool.get_data_byte_vec().clear();
	this->states.risk_set_data((State*)(bbase + base->blocks[0].offset));
	this->states.risk_set_size(size_t(base->total_states));
	this->states.risk_set_capacity(size_t(base->total_states));

	this->pool.get_data_byte_vec().risk_set_data(base->blocks[1].offset + bbase);
	this->pool.get_data_byte_vec().risk_set_size(size_t(base->blocks[1].length));
	this->pool.get_data_byte_vec().risk_set_capacity(size_t(base->blocks[1].length));

	this->firstFreeState = size_t(base->firstFreeState);
	this->numFreeStates  = size_t(base->numFreeStates);
	this->transition_num = size_t(base->transition_num);

	this->m_atom_dfa_num = base->atom_dfa_num;
	this->m_dfa_cluster_num = base->dfa_cluster_num;
}

long prepare_save_mmap(DFA_MmapHeader* base, const void** dataPtrs)
const override {
	base->firstFreeState = this->firstFreeState;
	base->numFreeStates  = this->numFreeStates ;
	base->transition_num = this->transition_num;
	base->state_size     = sizeof(State);
	base->num_blocks     = 2;
	base->blocks[0].offset = sizeof(DFA_MmapHeader);
	base->blocks[0].length = sizeof(State)*this->states.size();
	base->blocks[1].offset = sizeof(DFA_MmapHeader) + align_to_64(sizeof(State)*this->states.size());
	base->blocks[1].length = this->pool.size();
	dataPtrs[0] = this->states.data();
	dataPtrs[1] = this->pool.data();

	base->atom_dfa_num = this->m_atom_dfa_num;
	base->dfa_cluster_num = this->m_dfa_cluster_num;

	return 0;
}


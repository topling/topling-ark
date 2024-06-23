	enum { SERIALIZATION_VERSION = 2 };
	template<class DataIO> void load_au(DataIO& dio, size_t version) {
		typename DataIO::my_var_size_t stateBytes, n_states, n_zpath_states;
		dio >> stateBytes;
		dio >> n_zpath_states;
		dio >> n_states;
		if (version >= 2)
			load_kv_delim_and_is_dag(this, dio);
		if (sizeof(State) != stateBytes.t) {
			typedef typename DataIO::my_DataFormatException bad_format;
			TERARK_THROW(bad_format, "data.StateBytes=%d", (int)sizeof(State));
		}
		states.resize_no_init(n_states.t);
		dio.ensureRead(&states[0], sizeof(State) * n_states.t);
		m_zpath_states = size_t(n_zpath_states.t);
		if (version < 2) {
			// this->m_is_dag = tpl_is_dag();
			// m_kv_delim is absent in version < 2
		}
	}
	template<class DataIO> void save_au(DataIO& dio) const {
		dio << typename DataIO::my_var_size_t(sizeof(State));
		dio << typename DataIO::my_var_size_t(m_zpath_states);
		dio << typename DataIO::my_var_size_t(states.size());
		dio << uint16_t(this->m_kv_delim); // version >= 2
		dio << uint08_t(this->m_is_dag); // version >= 2
		dio.ensureWrite(states.data(), sizeof(State) * states.size());
	}
	template<class DataIO>
	friend void DataIO_loadObject(DataIO& dio, MyType& au) {
		typename DataIO::my_var_size_t version;
		dio >> version;
		if (version > SERIALIZATION_VERSION) {
			typedef typename DataIO::my_BadVersionException bad_ver;
			throw bad_ver(version.t, SERIALIZATION_VERSION, BOOST_CURRENT_FUNCTION);
		}
		au.load_au(dio, version.t);
	}
	template<class DataIO>
	friend void DataIO_saveObject(DataIO& dio, const MyType& au) {
		dio << typename DataIO::my_var_size_t(SERIALIZATION_VERSION);
		au.save_au(dio);
	}



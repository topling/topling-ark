/* vim: set tabstop=4 : */
// for const and non-const

	CompareT comp() TERARK_M_const { return m_index.key_comp(); }
	CompareT key_comp() TERARK_M_const { return m_index.key_comp(); }

	bool empty() TERARK_M_const { return m_index.empty(); }

	TERARK_M_iterator begin() TERARK_M_const { return m_index.begin(); }
	TERARK_M_iterator end()   TERARK_M_const { return m_index.end(); }

	TERARK_M_reverse_iterator rbegin()TERARK_M_const { return TERARK_M_reverse_iterator(end()); }
	TERARK_M_reverse_iterator rend()  TERARK_M_const { return TERARK_M_reverse_iterator(begin()); }

	template<typename CompatibleKey>
	TERARK_M_iterator find(const CompatibleKey& x) TERARK_M_const
	{
		return m_index.find(x);
	}
	template<typename CompatibleKey,typename CompatibleCompare>
	TERARK_M_iterator find(const CompatibleKey& x,CompatibleCompare comp) TERARK_M_const
	{
		return m_index.find(x, comp);
	}

	template<typename CompatibleKey>
	TERARK_M_iterator lower_bound(const CompatibleKey& x) TERARK_M_const
	{
		return m_index.lower_bound(x);
	}

	template<typename CompatibleKey,typename CompatibleCompare>
	TERARK_M_iterator lower_bound(const CompatibleKey& x,CompatibleCompare comp) TERARK_M_const
	{
		return m_index.lower_bound(x, comp);
	}

	template<typename CompatibleKey>
	TERARK_M_iterator upper_bound(const CompatibleKey& x) TERARK_M_const
	{
		return m_index.upper_bound(x);
	}

	template<typename CompatibleKey,typename CompatibleCompare>
	TERARK_M_iterator upper_bound(const CompatibleKey& x,CompatibleCompare comp) TERARK_M_const
	{
		return m_index.upper_bound(x,comp);
	}

	template<typename CompatibleKey>
	std::pair<TERARK_M_iterator,TERARK_M_iterator> equal_range(const CompatibleKey& x) TERARK_M_const
	{
		return m_index.equal_range(x);
	}

	template<typename CompatibleKey,typename CompatibleCompare>
	std::pair<TERARK_M_iterator,TERARK_M_iterator> equal_range(
		const CompatibleKey& x,CompatibleCompare comp) TERARK_M_const
	{
		return m_index.equal_range(x, comp);
	}

	key_type key(TERARK_M_iterator iter) TERARK_M_const
	{
		return raw_key(iter);
	}
	void key(TERARK_M_iterator iter, key_type& xkey) TERARK_M_const
	{
		xkey = raw_key(iter);
	}

	key_type key(TERARK_M_reverse_iterator iter) TERARK_M_const
	{
		return raw_key(iter);
	}
	void key(TERARK_M_reverse_iterator iter, key_type& xkey) TERARK_M_const
	{
		xkey = raw_key(iter);
	}


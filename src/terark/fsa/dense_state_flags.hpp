#pragma once

template<class StateID>
struct DenseStateFlags {
	StateID  m_is_pzip : 1;
	StateID  m_is_term : 1;
	StateID  m_is_free : 1;
	StateID  reserved_bits : sizeof(StateID)*8 - 3;

	DenseStateFlags() {
		m_is_pzip = 0;
		m_is_term = 0;
		m_is_free = 0;
		reserved_bits = 0;
		static_assert(sizeof(StateID) == sizeof(DenseStateFlags),
			"static_assert(sizeof(StateID) == sizeof(DenseStateFlags))");
	}

	void set_pzip_bit() { m_is_pzip = 1; }
	void set_term_bit() { m_is_term = 1; }
	void set_free_bit() { m_is_free = 1; }
	void clear_term_bit() { m_is_term = 0; }

	bool is_pzip() const { return m_is_pzip; }
	bool is_term() const { return m_is_term; }
	bool is_free() const { return m_is_free; }
};

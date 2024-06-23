#pragma once

#include <stdio.h>
#include <terark/fstring.hpp>
#include <terark/util/function.hpp>
#include <terark/valvec.hpp>

namespace terark {

struct DFA_MmapHeader;
class ByteInputRange;

class TERARK_DLL_EXPORT BaseAC {
protected:
	BaseAC(const BaseAC&);
	BaseAC& operator=(const BaseAC&);

public:
	typedef uint32_t word_id_t;
	typedef function<void(size_t endpos, const word_id_t* a, size_t n, size_t state)>
			OnHitWords; // handle multiple words in one callback

	BaseAC();
	virtual ~BaseAC();
	virtual void ac_scan(fstring input, const OnHitWords&) const = 0;
	virtual void ac_scan(fstring input, const OnHitWords&, const function<byte_t(byte_t)>&) const = 0;
	virtual void ac_scan(ByteInputRange&, const OnHitWords&) const = 0;
	virtual void ac_scan(ByteInputRange&, const OnHitWords&, const function<byte_t(byte_t)>&) const = 0;

	size_t num_words() const { return n_words; }

	bool has_word_length() const {
		assert(offsets.size() == 0 || offsets.size() == n_words + 1);
	   	return offsets.size() >= 1;
   	}

	bool has_word_content() const {
#ifndef NDEBUG
		if (offsets.empty()) {
			assert(strpool.empty());
		} else {
		   	assert(offsets.size() == n_words + 1);
			assert(strpool.size() == 0 || strpool.size() == offsets.back());
		}
#endif
	   	return !strpool.empty();
   	}

	size_t wlen(size_t wid) const {
		assert(offsets.size() == n_words + 1);
		assert(wid < n_words);
		size_t off0 = offsets[wid+0];
		size_t off1 = offsets[wid+1];
		return off1 - off0;
	}

	fstring word(size_t wid) const {
		// offsets and strpool are optional
		// if this function is called, offsets and strpool must be used
		assert(offsets.size() == n_words + 1);
		assert(offsets.back() == strpool.size());
		assert(wid < n_words);
		size_t off0 = offsets[wid+0];
		size_t off1 = offsets[wid+1];
		return fstring(strpool.data() + off0, off1 - off0);
	}

	std::string  restore_word(size_t stateID, size_t wID) const;
	virtual void restore_word(size_t stateID, size_t wID, std::string*) const;

protected:
	void ac_str_finish_load_mmap(const DFA_MmapHeader*);
	void ac_str_prepare_save_mmap(DFA_MmapHeader*, const void** dataPtrs) const;
	void assign(const BaseAC& y);
	void swap(BaseAC& y);

	typedef uint32_t offset_t;
	size_t n_words;
	valvec<offset_t> offsets; // optional
	valvec<char>     strpool; // optional
};

} // namespace terark



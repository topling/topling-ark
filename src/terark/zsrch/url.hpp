#pragma once

#include <terark/util/fstrvec.hpp>
#include <terark/hash_strmap.hpp>
#include <terark/lcast.hpp>
#include <terark/util/unicode_iterator.hpp>

namespace terark {

inline
void url_decode(fstring url, std::string* output) {
	output->resize(0);
	const char* p = url.begin();
	while (p < url.end()) {
		if ('%' == *p) {
			unsigned long byte = terark::hexlcast(p+1, 2);
			output->push_back((unsigned char)byte);
			p += 3;
		} else {
			output->push_back(*p++);
		}
	}
}

inline
std::string& url_encode(fstring url, std::string* output) {
	output->resize(0);
	const char* p = url.begin();
	while (p < url.end()) {
		if (isalnum((unsigned char)(*p)) || strchr("-_./", *p)) {
			output->push_back(*p);
		} else {
			output->resize(output->size()+3);
			sprintf(&*output->begin() + output->size() - 3, "%%%02X", (unsigned char)(*p));
		}
		++p;
	}
	return *output;
}

class UrlQueryParser {
	hash_strmap<fstrvec> args;
	std::string decode_buf;
public:
	void parse(fstring params) {
		args.erase_all();
		const char* beg = params.begin();
		const char* end = params.end();
		while (beg < end) {
			const char* amp = std::find(beg, end, '&');
			const char* equ = std::find(beg, amp, '=');
			fstring  val(equ==amp?amp:equ+1, amp);
			fstrvec& vals = args[fstring(beg, equ)];
			url_decode(val, &decode_buf);
			vals.emplace_back(decode_buf.data(), decode_buf.size());
			vals.back_append('\0');
			beg = amp + 1;
		}
		/*
		for(size_t i = 0; i < args.end_i(); ++i) {
			size_t n = args.val(i).size();
			FCGI_printf("i=%d args[%s].listlen=%d<br/>\n", int(i), args.key(i).c_str(), int(n));
			fstrvec& val = args.val(i);
			for(size_t j = 0; j < n; ++j) {
				FCGI_printf("args[%s][%d]: %s<br/>\n", args.key(i).c_str(), int(j), val.beg_of(j));
			}
		}
		*/
	}
	fstring get_uniq(fstring key) const {
		size_t idx = args.find_i(key);
		if (args.end_i() == idx) {
			return "";
		} else {
			fstring val = args.val(idx)[0];
			val.n--;
			return val;
		}
	}
	const hash_strmap<fstrvec>& get_args() const { return args; }
};

inline
size_t hanzi_bytes(fstring utf8, size_t pos) {
	if (utf8[pos] & 0x80)
		return terark::utf8_byte_count(utf8[pos]);
	else { // treat continuous ascii chars as one HanZi
		size_t i = pos;
		do ++i; while (i < utf8.size() && utf8[i] < 128);
		return i - pos;
	}
}

inline
size_t hanzi_count(fstring utf8) {
	size_t len = 0;
	for(size_t i = 0; i < utf8.size(); len++) {
		if (utf8[i] & 0x80)
			i += terark::utf8_byte_count(utf8[i]);
		else // treat continuous ascii chars as one HanZi
			do ++i; while (i < utf8.size() && utf8[i] < 128);
	}
	return len;
}

inline
bool is_all_hanzi(fstring utf8) {
	for(size_t i = 0; i < utf8.size(); ) {
		if (utf8[i] & 0x80)
			i += terark::utf8_byte_count(utf8[i]);
		else // treat continuous ascii chars as one HanZi
			return false;
	}
	return true;
}

} // namespace terark

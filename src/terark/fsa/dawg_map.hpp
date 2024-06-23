#pragma once

#include "fsa.hpp"
#include <terark/bitmap.hpp>
#include <terark/valvec.hpp>
#include <terark/util/linebuf.hpp>
#include <terark/util/mmap.hpp>
#include <terark/io/DataIO.hpp>
#include <terark/io/FileStream.hpp>
#include <terark/io/StreamBuffer.hpp>
#include <terark/num_to_str.hpp>

namespace terark {

template<class Value>
class DAWG_Map : boost::noncopyable {
	BaseDAWG* dawg;
	valvec<Value> values;
	void*  mmap_base; // for values
	size_t mmap_size;
	bool is_patched;

public:
	DAWG_Map(fstring dfaFile, fstring valueFile) {
		BaseDFA* dfa = BaseDFA::load_from(dfaFile);
		if (NULL == dfa) {
			THROW_STD(logic_error, "load dfa file %s failed", dfaFile.c_str());
		}
		dawg = dynamic_cast<BaseDAWG*>(dfa);
		if (NULL == dawg) {
			delete dfa;
			throw std::logic_error("DAWG_Map::DAWG_Map(DFA), not a dawg");
		}
		mmap_base = NULL;
		mmap_size = 0;
		is_patched = false;
		load_mmap_values(valueFile);
	}
	explicit DAWG_Map(BaseDFA* dfa) {
		dawg = dynamic_cast<BaseDAWG*>(dfa);
		if (NULL == dawg) {
			delete dfa;
			throw std::logic_error("DAWG_Map::DAWG_Map(DFA), not a dawg");
		}
		mmap_base = NULL;
		mmap_size = 0;
		is_patched = false;
	}
	DAWG_Map() {
		dawg = NULL;
		mmap_base = NULL;
		mmap_size = 0;
		is_patched = false;
	}
	~DAWG_Map() {
		if (mmap_base) {
			values.risk_release_ownership();
			terark::mmap_close(mmap_base, mmap_size);
		}
		delete dawg;
	}
	const BaseDAWG* get_dawg() const { return dawg; }

	size_t mem_size() const {
		const BaseDFA* dfa = dynamic_cast<const BaseDFA*>(dawg);
		return dfa->mem_size() + sizeof(Value) * values.size();
	}

	size_t size() const {
		assert(NULL != dawg);
		assert(values.size() == dawg->num_words());
	   	return values.size();
   	}

	std::string key(size_t nth) const {
		assert(NULL != dawg);
		return dawg->nth_word(nth);
   	}
	const Value& val(size_t nth) const {
		assert(NULL != dawg);
		assert(values.size() == dawg->num_words());
		assert(nth < values.size());
		return values[nth];
   	}
	Value& val(size_t nth) {
		assert(NULL != dawg);
		assert(values.size() == dawg->num_words());
		assert(nth < values.size());
		return values[nth];
   	}

	const Value* find(fstring key) const {
		assert(NULL != dawg);
		assert(values.size() == dawg->num_words());
		size_t nth = dawg->index(key);
		if (dawg->null_word == nth)
			return NULL;
		else
			return &values[nth];
	}
	Value* find(fstring key) {
		assert(NULL != dawg);
		assert(values.size() == dawg->num_words());
		size_t nth = dawg->index(key);
		if (dawg->null_word == nth)
			return NULL;
		else
			return &values[nth];
	}

	template<class OnKeyValue, class ValueVec>
	struct OnKeyValueOP {
		OnKeyValue onKeyVal;
		ValueVec values;
		void operator()(size_t nth, fstring key) const {
			assert(nth < values.size());
			onKeyVal(nth, key, values[nth]);
		}
		OnKeyValueOP(OnKeyValue onKV, ValueVec vv)
		   	: onKeyVal(onKV), values(vv) {}
	};

	// OnKeyValue(nth, key, value)
	template<class OnKeyValue>
	void for_each(OnKeyValue onKeyVal) const {
		const AcyclicPathDFA* dfa = dynamic_cast<const AcyclicPathDFA*>(dawg);
		if (NULL == dfa) {
			throw std::logic_error("Unexpected: DAWG is not an AcyclicPathDFA");
		}
		OnKeyValueOP<OnKeyValue, const valvec<Value>&> on_word0(onKeyVal, values);
		function<void(size_t nth, fstring)> on_word(ref(on_word0));
		dfa->for_each_word(on_word);
	}
	template<class OnKeyValue>
	void for_each(OnKeyValue onKeyVal) {
		const AcyclicPathDFA* dfa = dynamic_cast<const AcyclicPathDFA*>(dawg);
		if (NULL == dfa) {
			throw std::logic_error("Unexpected: DAWG is not a AcyclicPathDFA");
		}
		OnKeyValueOP<OnKeyValue, valvec<Value>&> on_word0(onKeyVal, values);
		function<void(size_t nth, fstring)> on_word(ref(on_word0));
		dfa->for_each_word(on_word);
	}

	// dawg_file will be appended the values
	template<class ParseLine>
	void complete_dawg_map(fstring text_key_value_file, fstring bin_dawg_file, ParseLine parse_line, bool overwriteValues = true) {
		using namespace terark;
		assert(NULL == dawg);
		assert('\0' == *text_key_value_file.end());
		assert('\0' == *bin_dawg_file.end());
		FileStream dawg_fp(bin_dawg_file.c_str(), "rb+");
		BaseDFA* dfa;
		{
			NativeDataInput<SeekableInputBuffer> dio; dio.attach(&dawg_fp);
			dfa = BaseDFA::load_from_NativeInputBuffer(&dio);
			dawg = dynamic_cast<BaseDAWG*>(dfa);
			if (NULL == dawg) {
				delete dfa;
				throw std::logic_error("DAWG_Map::load_from(), not a dawg");
			}
			long long dawg_dfa_bytes = dio.tell();
			long long dawg_file_size = dawg_fp.size();
			if (dawg_file_size != dawg_dfa_bytes && !overwriteValues) {
				string_appender<> msg;
				msg << "DAWG_Map::complete_dawg_map("
					<< text_key_value_file.c_str()
					<< ", "
					<< bin_dawg_file.c_str()
					<< ", parse_line, overwriteValues=false): "
					<< "dawg_file_size=" << dawg_file_size
					<< "dawg_dfa_bytes=" << dawg_dfa_bytes
					<< "\n";
				fputs(msg.c_str(), stderr);
				throw std::logic_error(msg);
			}
			dawg_fp.seek(dawg_dfa_bytes);
		}
		FileStream text_fp(text_key_value_file.c_str(), "r");
		patch_values<ParseLine>(text_fp, parse_line);
		NativeDataOutput<OutputBuffer> dio; dio.attach(&dawg_fp);
		dio << values;
	}

	template<class ParseLine>
	void complete_dawg_map_mmap(fstring text_key_value_file
							  , fstring bin_dawg_file
							  , fstring bin_value_file
							  , ParseLine parse_line
	) {
		BOOST_STATIC_ASSERT(boost::has_trivial_destructor<Value>::value);
		using namespace terark;
		assert(NULL == dawg);
		assert('\0' == *text_key_value_file.end());
		assert('\0' == *bin_dawg_file.end());
		BaseDFA* dfa = BaseDFA::load_mmap(bin_dawg_file.c_str());
		dawg = dynamic_cast<BaseDAWG*>(dfa);
		if (NULL == dawg) {
			delete dfa;
			throw std::logic_error("DAWG_Map::load_mmap(), not a dawg");
		}
		FileStream text_fp(text_key_value_file.c_str(), "r");
		patch_values<ParseLine>(text_fp, parse_line);
		FileStream value_fp(bin_value_file.c_str(), "wb");
		value_fp.ensureWrite(values.data(), sizeof(Value) * values.size());
	}

//	void patch_values(FILE* text_fp, const function<void(fstring line, fstring* pKey, Value* pVal)>& parse_line)
	template<class ParseLine>
	void patch_values(FILE* text_fp, ParseLine parse_line) {
		assert(NULL != dawg);
		assert(values.empty());
		values.resize(dawg->num_words());
		terark::LineBuf line;
		fstring theKey;
		Value theVal;
		febitvec bits(values.size(), 0, 1);
		while (line.getline(text_fp) > 0) {
			line.chomp();
			parse_line(fstring(line), &theKey, &theVal);
			size_t idx = dawg->index(theKey);
			if (dawg->null_word == idx) {
				string_appender<> msg;
				msg << __FILE__ << ":" << __LINE__ << ": dawg->index(";
				msg.append(line.p, line.n);
				msg << ")";
				throw std::logic_error(msg);
			}
			assert(idx < values.size());
			bits.set1(idx);
			values[idx] = theVal;
		}
		if (!bits.isall1()) {
			throw std::logic_error("some values were not padded");
		}
		is_patched = true;
	}
	void append_values_to_file(fstring fname) {
		assert(*fname.end() == '\0');
		assert(NULL != dawg);
		assert(values.size() == dawg->num_words());
		using namespace terark;
		FileStream fp(fname.c_str(), "ab");
		NativeDataOutput<OutputBuffer> dio; dio.attach(&fp);
		dio << values;
	}
	void load_map(fstring fname) {
		using namespace terark;
		assert(*fname.end() == '\0');
		FileStream fp(fname.c_str(), "rb");
		NativeDataInput<InputBuffer> dio; dio.attach(&fp);
		BaseDFA* dfa = BaseDFA::load_from_NativeInputBuffer(&dio);
		dawg = dynamic_cast<BaseDAWG*>(dfa);
		if (NULL == dawg) {
			delete dfa;
			throw std::logic_error("DAWG_Map::load_from(), not a dawg");
		}
		dio >> values;
		assert(values.size() == dawg->num_words());
		is_patched = true;
	}
	void save_map(fstring fname) {
		assert(*fname.end() == '\0');
		assert(NULL != dawg);
		assert(values.size() == dawg->num_words());
		using namespace terark;
		FileStream fp(fname.c_str(), "wb");
		BaseDFA* dfa = dynamic_cast<BaseDFA*>(dawg);
		assert(NULL != dfa);
		dfa->save_to(fp);
		NativeDataOutput<OutputBuffer> dio; dio.attach(&fp);
		dio << values;
	}

	void load_fdio_values(fstring fname) {
		assert(*fname.end() == '\0');
		assert(NULL != dawg);
		using namespace terark;
		FileStream fp(fname.c_str(), "rb");
		NativeDataInput<InputBuffer> dio; dio.attach(&fp);
		dio >> values;
	}

	void save_mmap_values(fstring fname) {
		assert(*fname.end() == '\0');
		assert(NULL != dawg);
		assert(values.size() == dawg->num_words());
		using namespace terark;
		FileStream value_fp(fname.c_str(), "wb");
		value_fp.ensureWrite(values.data(), sizeof(Value) * values.size());
	}

	void load_mmap_values(fstring value_mmap_file) {
		assert(NULL != dawg);
		size_t len = 0;
	   	mmap_base = terark::mmap_load(value_mmap_file.c_str(), &len);
		assert(sizeof(Value) * dawg->num_words() == len);
		if (sizeof(Value) * dawg->num_words() != len) {
			terark::mmap_close(mmap_base, len);
			mmap_base = NULL;
			throw std::logic_error("DAWG_Map::load_mmap_values");
		}
		values.clear();
		values.risk_set_data((Value*)mmap_base);
		values.risk_set_size(dawg->num_words());
		values.risk_set_capacity(dawg->num_words());
	}
};

/*
struct fstring;

// abstract interface
// could handle and sized value, but all value in a dawg_map are the same size
struct dawg_map_i {
	typedef function<void(      void* value)> OnExisted;
	typedef function<void(const void* found)> OnFound;
	typedef function<void(const fstring& key, void* val)> OnEachKV;

	virtual ~dawg_map_i();

	virtual bool insert(const fstring& word, const void* val, const OnExisted& on_existed) = 0;
	virtual bool find(const fstring& word, const OnFound& on_found);
	virtual void compact() = 0;

	virtual bool   empty() const = 0;
	virtual size_t size () const = 0;

	virtual bool for_each(const OnEachKV& on_each) = 0;

	static dawg_map_i* create(const char* state_class, size_t value_size, const char* tmpDataFile);
};

// thin template wrapper on dawg_map_i
// @note Value must be simple value, it must be memmovable
// if value has some allocated resource, must use for_each before destruct
template<class Value>
class dawg_map {
	std::auto_ptr<dawg_map_i> handle;

public:
	typedef function<void(      Value& value)> OnExisted;
	typedef function<void(const Value& value)> OnFound;
	typedef function<void(const fstring& key, Value& val)> OnEachKV;

	dawg_map(const char* state_class, const char* tmpDataFile) {
		handle.reset(dawg_map_i::create(state_class, sizeof(Value), tmpDataFile));
	}
	bool insert(const fstring& word, const Value& val, const OnExisted& on_existed) {
		return handle->insert(word, &val,
				reinterpret_cast<dawg_map_i::OnExisted>(on_existed));
	}
	bool find(const fstring& word, const OnFound& on_found) const {
		return handle->find(word, reinterpret_cast<dawg_map_i::OnFound>(on_found));
	}
	void compact() {
		handle->compact();
	}
	bool   empty() const { return handle->empty(); }
	size_t size () const { return handle->size (); }

	void for_each(const OnEachKV& on_each) {
		handle->for_each(reinterpret_cast<dawg_map_i::OnEachKV>(on_each));
	}
};
*/

} // namespace terark

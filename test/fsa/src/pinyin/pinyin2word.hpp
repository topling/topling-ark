#ifndef __terark_pinyin2word_hpp__
#define __terark_pinyin2word_hpp__

#include <terark/hash_strmap.hpp>
#include <terark/valvec.hpp>
#include <utility>
#include <boost/noncopyable.hpp>

using namespace terark;

class TransformerDecoder : private boost::noncopyable {
public:
	typedef unsigned int uint;
	class ac_t;
protected:
	class ac_t *dict1, *dict2;
	valvec<char> strpool1, strpool2;
	valvec<uint> offsets1, offsets2;
	valvec<uint> map1to2, map1to2_pool; // 1 word   to many PinYin
	valvec<uint> map2to1, map2to1_pool; // 1 PinYin to many word

	static
	std::pair<const uint*, const uint*>
	find_x(const fstring str, const ac_t& dict
		 , const valvec<uint>& offsets
		 , const valvec<uint>& map
		 , const valvec<uint>& mappool);

public:
	struct Edge {
		uint len;
		uint kid;
	//	double weight;
	};
	class TranverseCTX;

	TransformerDecoder();
	~TransformerDecoder();

	std::pair<const uint*, const uint*>	find_1to2(const fstring str) const;
	std::pair<const uint*, const uint*>	find_2to1(const fstring str) const;

	bool is_empty() { return strpool1.size() + strpool2.size() == 0; }
	void swap(TransformerDecoder& y);
	bool load(const char* fpath);

	bool load_binary(const char* fpath);

	template<class OnWord1, class OnWord2> TranverseCTX* make_1to2(const OnWord1& on1, const OnWord2& on2) const;
	template<class OnWord1, class OnWord2> TranverseCTX* make_2to1(const OnWord1& on1, const OnWord2& on2) const;

	void expand_new_words_pinyin(fstring wordfile);

	int main(int argc, char* argv[]);
};

class TransformerDecoder::TranverseCTX {
	friend class TransformerDecoder;
	const ac_t* dict; // dict2 or dict1
	const uint* map;  // map2to1      or map1to2
	const uint* pool; // map2to1_pool or map1to2_pool
	const uint* offsets1;
	const char* strpool1;
	const uint* offsets2;
	const char* strpool2;
	valvec<valvec<Edge> > forward1;
	valvec<valvec<uint> > forward2;
	valvec<uint>  seq1;
	valvec<uint>  path1;
	valvec<uint>  path2;
public:
	std::string   word1; // working buf
	std::string   word2; // working buf
	int output_sep_ch1 = -1;
	int output_sep_ch2 = -1;

	TranverseCTX();
	virtual ~TranverseCTX();

	virtual void on_word1(const char* word1, const size_t wlen1) const = 0;
	virtual void on_word2(const char* word2, const size_t wlen2) const = 0;

	void scan_build(const fstring str);
	void scan_expand(const fstring str);
	void scan_bfs_shortest(const fstring str);
	void discover1();
	void loop1(size_t pos1);
	void loop2(size_t pos2);
};

template<class OnWord1, class OnWord2>
class TranverseCTX_impl : public TransformerDecoder::TranverseCTX {
	OnWord1  f_on_word1;
	OnWord2  f_on_word2;
	void on_word1(const char* word1, size_t wlen1) const {
		f_on_word1(word1, wlen1);
	}
	void on_word2(const char* word2, size_t wlen2) const {
		f_on_word2(word2, wlen2);
	}
public:
	TranverseCTX_impl(const OnWord1& on1, const OnWord2& on2)
	   	: f_on_word1(on1), f_on_word2(on2) {}
};
template<class OnWord1, class OnWord2>
TransformerDecoder::TranverseCTX* TransformerDecoder::make_1to2(const OnWord1& on1, const OnWord2& on2) const {
	TranverseCTX* p = new TranverseCTX_impl<OnWord1, OnWord2>(on1, on2);
	p->dict = dict1;
	p->map  = map1to2.data();
	p->pool = map1to2_pool.data();
	p->offsets1 = offsets1.data();
	p->strpool1 = strpool1.data();
	p->offsets2 = offsets2.data();
	p->strpool2 = strpool2.data();
	p->output_sep_ch1 = '|';
	p->output_sep_ch2 = '|';
	return p;
}

template<class OnWord1, class OnWord2>
TransformerDecoder::TranverseCTX* TransformerDecoder::make_2to1(const OnWord1& on1, const OnWord2& on2) const {
	TranverseCTX* p = new TranverseCTX_impl<OnWord1, OnWord2>(on1, on2);
	p->dict = dict2;
	p->map  = map2to1.data();
	p->pool = map2to1_pool.data();
	p->offsets1 = offsets2.data();
	p->strpool1 = strpool2.data();
	p->offsets2 = offsets1.data();
	p->strpool2 = strpool1.data();
	p->output_sep_ch1 = '|';
	p->output_sep_ch2 = '|';
	return p;
}

#endif // __terark_pinyin2word_hpp__


#ifndef __terark_zsrch_document_parser_hpp__
#define __terark_zsrch_document_parser_hpp__

namespace terark { namespace zsrch {

class DocParser {
public:
	virtual ~DocParser() {}
	virtual bool parse(fstring data, hash_strmap<hash_strmap<uint32_t> >* docFields) = 0;
};

typedef DocParser* (*DocParserFactory)(fstring conf);

}} // namespace terark::zsrch

#endif // __terark_zsrch_document_parser_hpp__


/*
 * record_codec.h
 *
 *  Created on: Aug 27, 2015
 *      Author: leipeng
 */

#ifndef SRC_RECORD_CODEC_H_
#define SRC_RECORD_CODEC_H_

#include <mongo/bson/bsonobj.h>
#include <terark/valvec.hpp>
#include <terark/fstring.hpp>

namespace mongo {

void terarkEncodeRecord(const BSONObj& key, terark::valvec<char>* encoded);
terark::valvec<char> terarkEncodeRecord(const BSONObj& key);

BSONObj terarkDecodeRecord(const char* data, size_t size);
BSONObj terarkDecodeRecord(const terark::valvec<char>& encoded);
BSONObj terarkDecodeRecord(StringData encoded);
BSONObj terarkDecodeRecord(terark::fstring encoded);

}



#endif /* SRC_RECORD_CODEC_H_ */

/*
 * index_key.h
 *
 *  Created on: Jul 25, 2015
 *      Author: leipeng
 */

#ifndef INDEX_KEY_H_
#define INDEX_KEY_H_

#include <mongo/bson/bsonobj.h>
#include <terark/valvec.hpp>

namespace mongo {

class TerarkBsonBlob : public terark::valvec<char> {
	template<class DataIO>
	friend void DataIO_loadObject(DataIO& dio, TerarkBsonBlob& x) {
		int byteNum = dio.template load_as<int>();
		if (byteNum < 5) {
			fprintf(stderr, "TerarkBsonBlob::DataIO_loadObject: byteNum=%d\n", byteNum);
		}
		invariant(byteNum >= 5);
		x.resize_no_init(byteNum);
		DataView(x.data()).write<LittleEndian<int>>(byteNum);
		dio.ensureRead(x.data() + 4, byteNum - 4);
	}
	template<class DataIO>
	friend void DataIO_saveObject(DataIO& dio, const TerarkBsonBlob& x) {
		invariant(x.size() >= 5);
		invariant(int(x.size()) == *(const int*)x.data());
		dio.ensureWrite(x.data(), x.size());
	}
public:
//	using terark::valvec<char>;
};

TerarkBsonBlob terarkEncodeIndexKey(const BSONObj& key);
void terarkEncodeIndexKey(const BSONObj& key, TerarkBsonBlob* encoded);

BSONObj terarkDecodeIndexKey(StringData encoded, const BSONObj& fieldnames);

} // namespace mongo


#endif /* INDEX_KEY_H_ */

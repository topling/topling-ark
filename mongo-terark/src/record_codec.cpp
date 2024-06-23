/*
 * record_codec.cpp
 *
 *  Created on: Aug 27, 2015
 *      Author: leipeng
 */

#define MONGO_LOG_DEFAULT_COMPONENT ::mongo::logger::LogComponent::kStorage
#include <mongo/util/log.h>

#include "record_codec.h"
#include <mongo/bson/bsonobjbuilder.h>
#include <mongo/util/hex.h>
#include <terark/io/DataIO.hpp>
#include <terark/io/MemStream.hpp>

namespace mongo {

using namespace terark;

static void terarkEncodeBsonArray(const BSONObj& arr, valvec<char>& encoded);
static void terarkEncodeBsonObject(const BSONObj& obj, valvec<char>& encoded);

static
void terarkEncodeBsonElemVal(const BSONElement& elem, valvec<char>& encoded) {
	const char* value = elem.value();
	switch (elem.type()) {
	case EOO:
	case Undefined:
	case jstNULL:
	case MaxKey:
	case MinKey:
		break;
	case mongo::Bool:
		encoded.push_back(value[0] ? 1 : 0);
		break;
	case NumberInt:
		encoded.append(value, 4);
		break;
	case bsonTimestamp: // low 32 bit is always positive
	case mongo::Date:
	case NumberDouble:
	case NumberLong:
		encoded.append(value, 8);
		break;
	case jstOID:
	//	log() << "encode: OID=" << toHexLower(value, OID::kOIDSize);
		encoded.append(value, OID::kOIDSize);
		break;
	case Symbol:
	case Code:
	case mongo::String:
	//	log() << "encode: strlen+1=" << elem.valuestrsize() << ", str=" << elem.valuestr();
		encoded.append(value + 4, elem.valuestrsize());
		break;
	case DBRef:
		encoded.append(value + 4, elem.valuestrsize() + OID::kOIDSize);
		break;
	case mongo::Array:
		terarkEncodeBsonArray(elem.embeddedObject(), encoded);
		break;
	case Object:
		terarkEncodeBsonObject(elem.embeddedObject(), encoded);
		break;
	case CodeWScope:
		encoded.append(value, elem.objsize());
		break;
	case BinData:
		encoded.append(value, 5 + elem.valuestrsize());
		break;
	case RegEx:
		{
			const char* p = value;
			size_t len1 = strlen(p); // regex len
			p += len1 + 1;
			size_t len2 = strlen(p);
			encoded.append(p, len1 + 1 + len2 + 1);
		}
		break;
	default:
		{
			StringBuilder ss;
			ss << "terarkEncodeIndexKey(): BSONElement: bad elem.type " << (int)elem.type();
			std::string msg = ss.str();
			massert(10320, msg.c_str(), false);
		}
	}
}

static void terarkEncodeBsonArray(const BSONObj& arr, valvec<char>& encoded) {
	int cnt = 0;
	int itemType = 128;
	BSONForEach(item, arr) {
		if (itemType == 128) {
			itemType = item.type();
		} else {
			if (item.type() != itemType) itemType = 129;
		}
		cnt++;
	}
	{
		unsigned char  buf[8];
		encoded.append(buf, terark::save_var_uint32(buf, cnt));
	}
	if (cnt) {
		invariant(itemType != 128);
		encoded.push_back((unsigned char)(itemType));
		BSONForEach(item, arr) {
			if (itemType == 129) { // heterogeneous array items
				encoded.push_back((unsigned char)item.type());
			}
			terarkEncodeBsonElemVal(item, encoded);
		}
	}
}

static void terarkEncodeBsonObject(const BSONObj& obj, valvec<char>& encoded) {
	BSONForEach(elem, obj) {
		encoded.push_back((unsigned char)(elem.type()));
		encoded.append(elem.fieldName(), elem.fieldNameSize());
		terarkEncodeBsonElemVal(elem, encoded);
	}
	encoded.push_back((unsigned char)EOO);
}

void terarkEncodeRecord(const BSONObj& key, valvec<char>* encoded) {
	encoded->resize(0);
	terarkEncodeBsonObject(key, *encoded);
}
terark::valvec<char> terarkEncodeRecord(const BSONObj& key) {
	terark::valvec<char> encoded;
	terarkEncodeBsonObject(key, encoded);
	return encoded;
}

typedef LittleEndianDataOutput<AutoGrownMemIO> MyBufBuilder;

static void terarkDecodeBsonObject(MyBufBuilder& bb, const char*& pos, const char* end);
static void terarkDecodeBsonArray(MyBufBuilder& bb, const char*& pos, const char* end);

static void terarkDecodeBsonElemVal(MyBufBuilder& bb, const char*& pos, const char* end, int type) {
		switch (type) {
		case EOO:
			invariant(!"terarkDecodeBsonElemVal: encountered EOO");
			break;
		case Undefined:
		case jstNULL:
		case MaxKey:
		case MinKey:
			break;
		case mongo::Bool:
			bb << char(*pos ? 1 : 0);
			pos++;
			break;
		case NumberInt:
			bb.ensureWrite(pos, 4);
			pos += 4;
			break;
		case bsonTimestamp:
		case mongo::Date:
		case NumberDouble:
		case NumberLong:
			bb.ensureWrite(pos, 8);
			pos += 8;
			break;
		case jstOID:
			bb.ensureWrite(pos, OID::kOIDSize);
			pos += OID::kOIDSize;
			break;
		case Symbol:
		case Code:
		case mongo::String:
			{
				size_t len = strlen(pos);
				bb << int(len + 1);
				bb.ensureWrite(pos, len + 1);
			//	log() << "decode: strlen=" << len << ", str=" << pos;
				pos += len + 1;
			}
			break;
		case DBRef:
			{
				size_t len = strlen(pos);
				bb << int(len + 1);
				bb.ensureWrite(pos + 4, len + 1 + OID::kOIDSize);
				pos += len + 1 + OID::kOIDSize;
			}
			break;
		case mongo::Array:
			terarkDecodeBsonArray(bb, pos, end);
			break;
		case Object:
			terarkDecodeBsonObject(bb, pos, end);
			break;
		case CodeWScope:
			{
				int len = ConstDataView(pos).read<LittleEndian<int>>();
				bb.ensureWrite(pos, len);
				pos += len;
			}
			break;
		case BinData:
			{
				int len = ConstDataView(pos).read<LittleEndian<int>>();
				bb << len;
				bb << pos[4]; // binary data subtype
				bb.ensureWrite(pos + 5, len);
				pos += 5 + len;
			}
			break;
		case RegEx:
			{
				size_t len1 = strlen(pos); // regex len
				size_t len2 = strlen(pos + len1 + 1);
				size_t len3 = len1 + len2 + 2;
				bb.ensureWrite(pos, len3);
				pos += len3;
			}
			break;
		default:
			{
				StringBuilder ss;
				ss << "terarkDecodeIndexKey(): BSONElement: bad subkey.type " << (int)type;
				std::string msg = ss.str();
				massert(10320, msg.c_str(), false);
			}
		}

}

static void terarkDecodeBsonObject(MyBufBuilder& bb, const char*& pos, const char* end) {
	int byteNumOffset = bb.tell();
	bb << 0; // reserve 4 bytes for object byteNum
	for (;;) {
		if (pos >= end) {
			THROW_STD(invalid_argument, "Invalid encoded bson object");
		}
		const int type = (unsigned char)(*pos++);
		bb << char(type);
		if (type == EOO)
			break;
		StringData fieldname = pos;
		bb.ensureWrite(fieldname.begin(), fieldname.size()+1);
		pos += fieldname.size() + 1;
		terarkDecodeBsonElemVal(bb, pos, end, type);
	}
	int objByteNum = int(bb.tell() - byteNumOffset);
//	log() << "byteNumOffset" << byteNumOffset << ", objByteNum=" << objByteNum;
	(int&)bb.buf()[byteNumOffset] = objByteNum;
}

static void terarkDecodeBsonArray(MyBufBuilder& bb, const char*& pos, const char* end) {
	int cnt = terark::load_var_uint32((unsigned char*)pos, (const unsigned char**)&pos);
	if (0 == cnt) {
		bb << int(5); // 5 is empty bson object size
		bb << char(EOO);
		return;
	}
	int arrItemType = (unsigned char)(*pos++);
	int arrByteNumOffset = bb.tell();
	bb << int(0); // reserve for arrByteNum
	for (int arrIndex = 0; arrIndex < cnt; arrIndex++) {
		if (pos >= end) {
			THROW_STD(invalid_argument, "Invalid encoded bson array");
		}
		const int curItemType = arrItemType == 129 ? (unsigned char)(*pos++) : arrItemType;
		bb << char(curItemType);
		std::string idxStr = BSONObjBuilder::numStr(arrIndex);
		bb.ensureWrite(idxStr.c_str(), idxStr.size()+1);
		terarkDecodeBsonElemVal(bb, pos, end, curItemType);
	}
	bb << char(EOO);
	int arrByteNum = bb.tell() - arrByteNumOffset;
//	log() << "arrByteNumOffset" << arrByteNumOffset << "arrByteNum=" << arrByteNum;
	(int&)bb.buf()[arrByteNumOffset] = arrByteNum;
}

BSONObj terarkDecodeRecord(const char* data, size_t size) {
	MyBufBuilder bb;
	const char* pos = data;
	bb.resize(size);
	bb.skip(sizeof(SharedBuffer::Holder));
	terarkDecodeBsonObject(bb, pos, data + size);
	invariant(pos == data + size);
	return BSONObj::takeOwnership((char*)bb.release());
}

BSONObj terarkDecodeRecord(const terark::valvec<char>& encoded) {
	return terarkDecodeRecord(encoded.begin(), encoded.size());
}

BSONObj terarkDecodeRecord(StringData encoded) {
	return terarkDecodeRecord(encoded.begin(), encoded.size());
}

BSONObj terarkDecodeRecord(terark::fstring encoded) {
	return terarkDecodeRecord(encoded.begin(), encoded.size());
}

}


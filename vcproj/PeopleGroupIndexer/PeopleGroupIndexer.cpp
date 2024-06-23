// PeopleGroup.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <terark/io/FileStream.hpp>
#include <terark/rockeet/index/Index.hpp>
#include <terark/rockeet/store/DataReader.hpp>

using namespace terark;
using namespace terark::rockeet;

struct UserInfo
{
	unsigned long long uid; // varint
	unsigned long long interest;
	unsigned char sex;
	unsigned char stage;
	unsigned char grade;
	unsigned char padding[5];
};

int main(int argc, char* argv[])
{
	if (argc <= 3) {
		return 1;
	}
	FileStream input(argv[1], "r");

	Index index;
	index.addField("birth_year", DeltaInvertIndex::builtInFixedKeyIndex(tev_uint64));
	index.addField("school", DeltaInvertIndex::builtInFixedKeyIndex(tev_uint64));
	index.addField("major", DeltaInvertIndex::builtInFixedKeyIndex(tev_uint64));
	index.addField("area", DeltaInvertIndex::builtInFixedKeyIndex(tev_uint64));

	FileStream stored(argv[2], "wb");
//	stored.disbuf();

	for (;;)
	{
		int sex, stage, grade;
		int birth_year, birth_month, birth_day;
		unsigned long long area, school, major, interest;
		UserInfo ui;
		int fields = fscanf(input, "%llu\t%d|%d|%d|%d|%d|%llu|%llu|%llu|%llu\n"
			, &ui.uid, &sex, &birth_year, &birth_month, &birth_day, &stage, &area, &school, &grade, &major, &interest);
		if (fields == 11) {
			DocTermVec doc;
			doc.addField<unsigned long long>("birth_year", birth_year);
			doc.addField<unsigned long long>("school", school);
			doc.addField<unsigned long long>("major", major);
			doc.addField<unsigned long long>("area", area);
			unsigned long long docID = index.append(doc);
			ui.sex = sex;
			ui.stage = stage;
			ui.grade = grade;
			stored.ensureWrite(&ui, sizeof(ui));
		} else {
			fprintf(stderr, "input error, fields=%d\n", fields);
		}
	}
	return 0;
}


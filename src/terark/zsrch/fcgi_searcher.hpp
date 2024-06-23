#pragma once

#include <terark/lcast.hpp>
#include <terark/util/autoclose.hpp>
#include <terark/util/profiling.hpp>
#include <terark/util/linebuf.hpp>
#include <terark/util/unicode_iterator.hpp>
#include <terark/zsrch/zsrch.hpp>
#include <getopt.h>
#include "url.hpp"

#if defined(_MSC_VER)
	int FCGI_Accept_cnt = 1;
	#define FCGI_printf printf
	#define FCGI_Accept() FCGI_Accept_cnt--
	#define FCGI_stdout stdout
	#define FCGI_fwrite fwrite
	#define popen _popen
	#define pclose _pclose
#else
	#define NO_FCGI_DEFINES
	#include <fcgi_stdio.h>
#endif

using namespace terark;
using namespace terark::zsrch;

int FCGI_ExecuteSearcherLoop(IndexReader* indexReader)
try{

UrlQueryParser urlqry;
std::string encode_buf;
long requestNum = 0;
while (FCGI_Accept() >= 0) {
	requestNum++;
	FCGI_printf("Content-type: text/xml\r\n\r\n");
	char* params = getenv("QUERY_STRING");
	if (!params) {
		FCGI_printf("missing query word, by url param 'q'\n");
		continue;
	}
	urlqry.parse(params);
	fstring strqry = urlqry.get_uniq("q"); // q=field:regex
	int bShowAll = lcast(urlqry.get_uniq("docData"));
	terark::zsrch::QueryPtr qry(Query::createQuery(strqry));
	valvec<uint32_t> termList, docList;
	indexReader->getTermList(qry.get(), &termList);
	FCGI_printf(R"EOS(<?xml version="1.0" encoding="utf-8"?>)EOS""\n"
				 "<body>\n"
				 "  <requestNum>%ld</requestNum>\n"
				 "  <query><![CDATA[%.*s]]></query>\n"
			   , requestNum, strqry.ilen(), strqry.data()
			   );
	if (termList.empty()) {
		FCGI_printf("  <status>NotFound</status>\n");
	}
	else {
		FCGI_printf("  <status>OK</status>\n");
		FCGI_printf("  <matchedTerms>\n");
		std::string term;
		for (size_t i = 0; i < termList.size(); ++i) {
			long termID = termList[i];
			long docCnt = indexReader->getDocListLen(termID);
			indexReader->getTermString(termID, &term);
			FCGI_printf("    <term>\n");
			FCGI_printf("       <id>%ld</id>\n", termID);
			FCGI_printf("       <docCount>%ld</docCount>\n", docCnt);
			FCGI_printf("       <str><![CDATA[%s]]></str>\n", term.c_str());
			FCGI_printf("    </term>\n");
		}
		FCGI_printf("  </matchedTerms>\n");
		if (bShowAll) {
			indexReader->getDocList(termList, &docList);
			FCGI_printf("  <docCount>%ld</docCount>\n", long(docList.size()));
			FCGI_printf("  <documents>\n");
			DocMeta docMeta;
			std::string content;
			for (size_t i = 0; i < docList.size(); ++i) {
				long docID = docList[i];
				size_t lineno = size_t(-1);
				indexReader->getDocMeta(docID, &docMeta);
				indexReader->getDocData(docID, &content);
				FCGI_printf(
					"    <document>\n"
					"      <id>%ld</id>\n"
					"      <host><![CDATA[%s]]></host>\n"
					"      <file><![CDATA[%s]]></file>\n"
					"      <line>%ld</line>\n"
					"      <content><![CDATA[%s]]></content>\n"
					"    </document>\n"
					, docID, docMeta.host.c_str(), docMeta.path.c_str()
					, long(lineno + 1) // 0-based to 1-based
					, content.c_str()
					);
			}
			FCGI_printf("  </documents>\n");
		}
	}
	FCGI_printf("</body>\n");
	FCGI_Finish();
}
	return 0;
}
catch (const std::exception& ex) {
	return 3;
}


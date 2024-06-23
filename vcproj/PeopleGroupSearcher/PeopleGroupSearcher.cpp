// PeopleGroup.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <terark/rockeet/index/Index.hpp>
#include <terark/rockeet/store/DataReader.hpp>
#include <terark/rockeet/search/Searcher.hpp>

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

	IndexPtr index = Index::fromFile("index.idx");
	const char* historyFile = "history.txt";
	read_history(historyFile);

	using namespace boost::spirit;

	OrExpression topExpr;
	AndExpression* andExpr;
	QueryExpressionPtr pBottomExpr;

	unsigned minMatch;
	rule<> expr, exOr, exAnd, exXor, exSub, exMinMatch, term, field;
	expr  = +exOr;//[push_back_a(topExpr.subs, andExpr)];
	exOr  = exAnd >> *((ch_p('|') | "OR" ) >> exAnd);
	exAnd = exXor >> *((ch_p('+') | "AND") >> exXor);
	exXor = exSub >> *((ch_p('^') | "XOR") >> exSub);
	exSub = exMinMatch >> *('-' >> exMinMatch);
	exMinMatch = (str_p("MinMatch") | "mm")
			  >> '(' >> term >> uint_p[assign_a(minMatch)];
	term = '(' >> expr >> ')'
		 | field >> ':' >> confix_p('"', *c_escape_ch_p, '"');

	for (;;)
	{
		char* line = readline("query> ");

		add_history(line);
		free(line);
	}
	write_history(historyFile);
	return 0;
}


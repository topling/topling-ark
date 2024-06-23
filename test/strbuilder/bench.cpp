#include <stdlib.h>

#include <vector>
#include <iomanip>
#include <sstream>

#include <terark/util/autofree.hpp>
#include <terark/util/profiling.cpp>
#include <terark/util/strbuilder.cpp>

using namespace terark;

int main(int argc, char** argv)
{
	int loop = argc >= 2 ? atoi(argv[1]) : 1000;
	terark::profiling pf;
	std::vector<long long>   tv; tv.reserve(100);
	std::back_insert_iterator<std::vector<long long> >
		bi = std::back_inserter(tv);

	bi = pf.now();
	// build string in one line
	for (int i = 0; i < loop; ++i) {
		std::string smallstr = StrPrintf("%s %d\n", "abc", 100);
	}
	bi = pf.now();
	for (int i = 0; i < loop; ++i) {
		std::string smallstr = StrBuilder().printf("%s %d\n", "abc", 100);
	}
	bi = pf.now();

	// for big strings, such as sql query
	{
		std::string str("insert into tab(id1, id2) values ");
		for (int i = 0; i < loop; ++i)
			StrPrintf(str, "(%03d %03d),", i, i*i);
		str.resize(str.size()-1);
		str.append(";\n");
		std::string bigstr(str.begin(), str.end());
	}
	bi = pf.now();
	{
		StrBuilder sb;
		sb.printf("insert into tab(id1, id2) values ");
		for (int i = 0; i < loop; ++i) {
			sb.printf("(%03d %03d),", i, i*i);
		}
		sb.setEof(-1);  // trim last ','
		sb.printf(";\n"); // append a ';'
	//  sb.setEof(-1, ";\n"); // trim last ',' and append a ';'
		std::string bigstr = sb;
	}
	bi = pf.now();
	{
		using namespace std; // for setw setfill
		std::ostringstream oss;
		oss << "insert into tab(id1, id2) values ";
		for (int i = 0; i < loop; ++i)
			oss << "(" << setw(3) << setfill('0') << i
				<< " " << setw(3) << setfill('0') << i*i
				<< "),";
		oss.seekp(-1, oss.end); // trim last ',', oss.end is a static member: ios::end
		oss << ";"; // append a ';'
		std::string bigstr = oss.str();
	}
	bi = pf.now();
	{
		size_t len = 256;
		terark::AutoFree<char> buf(len);
		std::string str("insert into tab(id1, id2) values ");
		for (int i = 0; i < loop; ++i) {
			int rv = snprintf(buf, len, "(%03d %03d),", i, i*i);
			if (rv < 0) {
				perror("snprintf");
				abort();
			}
			str.append(buf, rv);
		}
		str.append(";\n");
	}
	bi = pf.now();

	printf("string once by StrPrintf  : tm=%10.6f'ms, P/B=%f\n", pf.ns(tv[0], tv[1])/1e6, (double)pf.ms(tv[0], tv[1])/pf.ms(tv[1],tv[2]));
	printf("string once by StrBuilder : tm=%10.6f'ms, B/P=%f\n", pf.ns(tv[1], tv[2])/1e6, (double)pf.ms(tv[1], tv[2])/pf.ms(tv[0],tv[1]));

	printf("string big  by StrPrintf  : tm=%10.6f'ms, P/B=%f\n", pf.ns(tv[2], tv[3])/1e6, (double)pf.ms(tv[2], tv[3])/pf.ms(tv[3],tv[4]));
	printf("string big  by StrBuilder : tm=%10.6f'ms, B/P=%f\n", pf.ns(tv[3], tv[4])/1e6, (double)pf.ms(tv[3], tv[4])/pf.ms(tv[2],tv[3]));
	printf("string big  by StrStream  : tm=%10.6f'ms, P/S=%f, B/S=%f\n", pf.ns(tv[4], tv[5])/1e6, (double)pf.ms(tv[2], tv[3])/pf.ms(tv[4],tv[5]), (double)pf.ms(tv[3], tv[4])/pf.ms(tv[4],tv[5]));
	printf("string big  by snprintf   : tm=%10.6f'ms, F/S=%f, F/B=%f\n", pf.ns(tv[5], tv[6])/1e6, (double)pf.ms(tv[5], tv[6])/pf.ms(tv[4],tv[5]), (double)pf.ms(tv[5], tv[6])/pf.ms(tv[3],tv[4]));

	return 0;
}


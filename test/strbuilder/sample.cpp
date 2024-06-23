#include <stdlib.h>
#include <iostream>
#include <sstream>
#include <iomanip>

#include <terark/util/strbuilder.cpp>

using namespace terark;

void print(const std::string& str) {
	std::cout << "+++\n";
	std::cout << str;
	std::cout << "---\n";
}

int main(int argc, char** argv)
{
  {// build small string by calling StrPrintf multiple times
    std::string smallstr;
    StrPrintf(smallstr, "%s %d\n", "abc", 100);
    StrPrintf(smallstr, "%s %d\n", "bcd", 101);
	print(smallstr);
  }
  { //  build string in one line
    std::string smallstr = StrPrintf("%s %d\n", "abc", 100);
	print(smallstr);
  }
  { // bare usage of gnu.asprintf, same as  build string in one line
	terark::AutoFree<char> s;
    int n = asprintf(&s.p, "%s %d\n", "abc", 100);
    if (n < 0) {
       perror("asprintf");
       abort(); // faital
    }
    std::string smallstr(s.p, n);
	print(smallstr);
  }
  //-------------------------------------------------
  // build string by StrBuilder
  { //  build string in one line
    std::string smallstr = StrBuilder().printf("%s %d\n", "abc", 100).flush();
	print(smallstr);
  }
  { // for big strings, such as sql query
    StrBuilder sb;
    sb.printf("insert into tab(id1, id2) values ");
    for (int i = 0; i < 100; ++i)
       sb.printf("(%03d %03d),", i, i*i);
    sb.setEof(-1);  // trim last ','
    sb.printf(";\n"); // append a ';'
    // sb.setEof(-1, ";\n"); // trim last ',' and append a ';'
    std::string bigstr = sb;
	print(bigstr);
  }
  {
    using namespace std; // for setw setfill
    std::ostringstream oss;
    for (int i = 0; i < 100; ++i)
        oss << "(" << setw(3) << setfill('0') << i
            << " " << setw(3) << setfill('0') << i*i
            << "),";
    oss.seekp(-1, oss.end); // trim last ',', oss.end is a static member: ios::end
    oss << ";"; // append a ';'
    std::string bigstr = oss.str();
	print(bigstr);
  }
  return 0;
}


#include <terark/num_to_str.hpp>
#include <terark/util/function.hpp>

using namespace terark;

int main(int argc, char* argv[]) {
	using fmt = string_op_fmt<string_appender<> >;
	string_appender<> str;
	fmt(str)^"%.*s"^"abcd"; TERARK_VERIFY_S_EQ(str, fstring("%.*sabcd"));
	fmt(str)^"\n";
	fmt(str)^"%8.3f\n"^8.3;
	fmt(str)^"%1.0d\n"; TERARK_VERIFY_S_EQ(fstring(str).suffix(6), "%1.0d\n");
	fmt(str)^"argc=%08d"^argc^" default fmt argc = "^argc;
	printf("%s\n", str.c_str());

	str.clear();
	str|ReplaceChar<>{".a.b.c..d++", '.', '-'};
	TERARK_VERIFY_S_EQ(str, "-a-b-c--d++");

	str.clear();
	str|ReplaceSubStr<>{".a.b.c..d++", ".", "-"};
	TERARK_VERIFY_S_EQ(str, "-a-b-c--d++");

	str.clear();
	str|ReplaceSubStr<>{".a.b.c..d++", "..", "---"};
	TERARK_VERIFY_S_EQ(str, ".a.b.c---d++");

	str.clear();
	str|ReplaceSubStr<>{".a.b.c...d++", "..", "---"};
	TERARK_VERIFY_S_EQ(str, ".a.b.c---.d++");

	str.clear();
	str|ReplaceSubStr<>{".a.b.c...d..", "..", "---"};
	TERARK_VERIFY_S_EQ(str, ".a.b.c---.d---");

	str.clear();
	str|ReplaceBySmallTR<>{".,a.,b.,c.,.d..", ".,", "-;"};
	TERARK_VERIFY_S_EQ(str, "-;a-;b-;c-;-d--");

	printf("%s OK!\n", argv[0]);
	return 0;
}

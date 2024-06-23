#include <terark/logged_tab.hpp>
#include <terark/gold_hash_map.hpp>

int main(int argc, char* argv[]) {
	if (argc < 3) {
		fprintf(stderr, "usage: %s dir read|write\n", argv[0]);
		return 1;
	}
	using namespace terark;
	typedef gold_hash_map<std::string, long> map_t;
	typedef Logged<map_t, std::string, long> logged_t;
	map_t map;
	logged_t logged(&map, argv[1]);
	if (strcmp(argv[2], "read") == 0) {
		for (size_t i = 0; i < map.size(); ++i) {
			printf("%s\t%ld\n", map.key(i).c_str(), map.val(i));
		}
	}
	else {
		long n1 = 5, n2 = 9;
		{
			logged_t::Transaction trx(&logged);
			for (long i = 0; i < n1; ++i) {
				char key[64];
				sprintf(key, "-%08ld", i);
				trx.set(key, i);
			}
			trx.commit();
		}
		logged.make_check_point();
		{
			logged_t::Transaction trx(&logged);
			for (long i = n1; i < n2; ++i) {
				char key[64];
				sprintf(key, "+%08ld", i);
				trx.set(key, i);
			}
			trx.commit();
		}
	}
	return 0;
}


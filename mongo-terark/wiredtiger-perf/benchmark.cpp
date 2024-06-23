#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <wiredtiger.h>
#include <getopt.h>
#include <terark/util/autofree.hpp>
#include <terark/util/autoclose.hpp>
#include <terark/util/profiling.cpp>
#include <terark/valvec.hpp>
#include <sys/stat.h>

typedef unsigned long long ull;

void usage(const char* prog) {
	fprintf(stderr, "usage: %s [-C cache_size] [-d database_dir] bsonFile\n", prog);
	exit(1);
}

int main(int argc, char* argv[]) {
	const char* cacheSize = NULL;
	const char* home = getenv("WIREDTIGER_HOME");
	for (;;) {
		int opt = getopt(argc, argv, "C:d:");
		switch (opt) {
		case -1:
			goto GetoptDone;
		case 'C':
			cacheSize = optarg;
			break;
		case 'd':
			home = optarg;
			break;
		}
	}
GetoptDone:
	const char* bsonFname = NULL;
	if (optind < argc) {
		bsonFname = argv[optind];
	} else {
		fprintf(stderr, "missing bsonFile\n");
		usage(argv[0]);
	}
	terark::Auto_fclose fp(fopen(bsonFname, "rb"));
	if (!fp) {
		fprintf(stderr, "Error: fopen(%s) = %s\n", bsonFname, strerror(errno));
		return 1;
	}
	if (NULL == home) {
		fprintf(stderr, "Error: database directory is missing\n");
		usage(argv[0]);
	}
	{
		struct stat st;
		if (stat(home, &st) == 0) {
		   	if (!S_ISDIR(st.st_mode)) {
				fprintf(stderr, "Error: database_dir=%s is not a diretory\n", home);
				return 1;
			}
		} else if (ENOENT == errno) {
			mkdir(home, 0755);
		} else {
			fprintf(stderr, "Error: stat(%s) = %s\n", home, strerror(errno));
			return 1;
		}
	}
	terark::AutoFree<char> confstr;
	asprintf(&confstr.p, "extensions=[/usr/lib/libwiredtiger_snappy.so],create,cache_size=%s", cacheSize);
	fprintf(stderr, "confstr=%s\n", confstr.p);

	WT_CONNECTION* conn = NULL;
	WT_CURSOR*     cursor = NULL;
	WT_SESSION*    session = NULL;
	int ret = 0;
	if ((ret = wiredtiger_open(home, NULL, confstr.p, &conn)) != 0) {
		fprintf(stderr, "Error connecting to %s: %s\n", home, wiredtiger_strerror(ret));
		return ret;
	}
	if ((ret = conn->open_session(conn, NULL, NULL, &session)) != 0) {
		fprintf(stderr, "Error open_session on %s: %s\n", home, wiredtiger_strerror(ret));
		return ret;
	}
	ret = session->create(session, "table:bson", "block_compressor=snappy,key_format=r,value_format=u");
	if (0 != ret) {
		fprintf(stderr, "Error create table %s on %s: %s\n", "bson", home, wiredtiger_strerror(ret));
		return ret;
	}
	ret = session->open_cursor(session, "table:bson", NULL, NULL, &cursor);
	if (0 != ret) {
		fprintf(stderr, "Error create cursor on table %s on %s: %s\n", "bson", home, wiredtiger_strerror(ret));
		return ret;
	}

	terark::profiling pf;
	terark::valvec<unsigned char> buf;
	ull recno = 1, cnt = 0, bytes = 0;
	ull t0 = pf.now();
	while (!feof(fp)) {
		int32_t reclen;
		if (fread(&reclen, 1, 4, fp) != 4) {
			break;
		}
		if (reclen <= 8) {
			fprintf(stderr, "read binary data error: recno=%lld reclen=%ld too small\n"
					, recno, long(reclen));
			break;
		}
		buf.resize(reclen-4);
		long datalen = fread(buf.data(), 1, reclen-4, fp);
		if (datalen != reclen-4) {
			fprintf(stderr, "read binary data error: recno=%lld requested=%ld returned=%ld\n"
					, recno, long(reclen-4), datalen);
			break;
		}
		WT_ITEM val = {0};
		val.data = buf.data();
		val.size = buf.size();
		cursor->set_key(cursor, recno);
		cursor->set_value(cursor, &val);
		ret = cursor->insert(cursor);
		if (0 != ret) {
			fprintf(stderr, "Error cursor insert(recno=%lld) on table %s on %s: %s\n", recno, "bson", home, wiredtiger_strerror(ret));
			return ret;
		}
		bytes += reclen-4;
		recno++;
	}
	cnt = recno-1;
	fprintf(stderr, "inserted %lld records\n", cnt);
	ull t1 = pf.now();
	cursor->reset(cursor);
	terark::valvec<uint32_t> idvec(cnt, terark::valvec_no_init());
	for (size_t i = 0; i < idvec.size(); ++i) idvec[i] = uint32_t(i);
	std::random_shuffle(idvec.begin(), idvec.end());
	ull t2 = pf.now();
	for (size_t i = 0; i < idvec.size(); ++i) {
		recno = idvec[i] + 1;
		cursor->set_key(cursor, recno);
		ret = cursor->search(cursor);
		if (0 != ret) {
			fprintf(stderr, "Error: cursor->search(%lld)\n", recno);
			return 1;
		}
		WT_ITEM val;
		ret = cursor->get_value(cursor, &val);
	}
	ull t3 = pf.now();
	while (cursor->next(cursor) == 0) {
		WT_ITEM val;
		ret = cursor->get_key(cursor, &recno);
		if (0 != ret) {
			fprintf(stderr, "Error: cursor->get_key\n");
			return 1;
		}
		ret = cursor->get_value(cursor, &val);
		if (0 != ret) {
			fprintf(stderr, "Error: cursor->get_value\n");
			return 1;
		}
	}
	ull t4 = pf.now();

	fprintf(stderr
		, "Read & Insert:  Time: %8.3f  mQPS: %8.3f  ns/Q: %8.1f  MB/s: %8.3f\n"
		, pf.sf(t0, t1)
		, cnt / pf.uf(t0, t1)
		, pf.nf(t0, t1) / cnt
		, bytes / pf.uf(t0, t1)
		);

	fprintf(stderr
		, "Random search:  Time: %8.3f  mQPS: %8.3f  ns/Q: %8.1f  MB/s: %8.3f\n"
		, pf.sf(t2, t3)
		, cnt / pf.uf(t2, t3)
		, pf.nf(t2, t3) / cnt
		, bytes / pf.uf(t2, t3)
		);

	fprintf(stderr
		, "Sequencial___:  Time: %8.3f  mQPS: %8.3f  ns/Q: %8.1f  MB/s: %8.3f\n"
		, pf.sf(t3, t4)
		, cnt / pf.uf(t3, t4)
		, pf.nf(t3, t4) / cnt
		, bytes / pf.uf(t3, t4)
		);

	conn->close(conn, NULL);
	return 0;
}

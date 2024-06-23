#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <terark/fsa/fsa.hpp>

const BaseDFA* g_dfa = NULL;
FILE* flog = NULL;

static int adfa_fuse_getattr(const char *path, struct stat *stbuf)
{
	MatchContext ctx = { 0, 0, 0, 0 };

	memset(stbuf, 0, sizeof(struct stat));
	if (strcmp(path, "/") == 0) {
		fprintf(flog, "getattr: %s\n", path);
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
		return 0;
	}
	try {
		fstring fpath = path;
		bool ret;
		ret = g_dfa->step_key_l(ctx, '/', fpath);
		fprintf(flog, "getattr: %s step_key_l(/)=%d: %zd %zd %zd %zd\n",
			   	path, ret, ctx.root, ctx.zidx, ctx.pos, ctx.nth);
		if (ret) {
			if (fpath.size() == ctx.pos) {
				stbuf->st_mode = S_IFDIR | 0755;
				stbuf->st_nlink = 2;
				return 0;
			}
		}
	   	ret = g_dfa->step_key_l(ctx, '\t', fpath);
		fprintf(flog, "getattr: %s step_key_l(\\t)=%d: %zd %zd %zd %zd\n",
			   	path, ret, ctx.root, ctx.zidx, ctx.pos, ctx.nth);
		if (ret) {
			if (fpath.size() == ctx.pos) {
				stbuf->st_mode = S_IFREG | 0444;
				stbuf->st_nlink = 1;
				stbuf->st_size = 4096;
				return 0;
			} else {
				return -ENOENT;
			}
		} else {
			return -ENOENT;
		}
	}
	catch (const std::exception& e) {
		fprintf(flog, "get_attr: exception: %s\n", e.what());
		return -ENOENT;
	}
}

/*
static int adfa_fuse_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{
	(void) offset;
	(void) fi;

	if (strcmp(path, "/") != 0)
		return -ENOENT;

	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);
//	filler(buf, adfa_fuse_path + 1, NULL, 0);

	return 0;
}
*/

static int adfa_fuse_open(const char *path, struct fuse_file_info *fi)
{
	if ((fi->flags & 3) != O_RDONLY)
		return -EACCES;

	MatchContext ctx = { 0, 0, 0, 0 };

	fprintf(flog, "fuse_open: %s\n", path);
	bool ret = g_dfa->step_key_l(ctx, '\t', path);
	fprintf(flog, "fuse_open: %s step_key_l(\\t)=%d: %zd %zd %zd %zd\n", path, ret, ctx.root, ctx.zidx, ctx.pos, ctx.nth);
   	if (ret) {
		fi->direct_io = 1;
		fi->nonseekable = 1;
		return 0;
	}
	return -ENOENT;
}

static int adfa_fuse_read(const char *path, char *buf, size_t size, off_t offset,
		      struct fuse_file_info *fi)
{
	size_t len;
	(void) fi;

	std::string valuebuf;
	fstring fpath(path);
	g_dfa->match_key_l('\t', fpath, [&](size_t keylen,size_t, fstring w) {
		if (fpath.size() == keylen) {
			valuebuf.append(w.data(), w.size());
			valuebuf.append("\n");
		}
	});
	len = valuebuf.size();
	if (offset < (off_t)len) {
		if (offset + size > len)
			size = len - offset;
		memcpy(buf, valuebuf.data() + offset, size);
	} else
		size = 0;

	return size;
}

static struct fuse_operations adfa_fuse_oper;

static void usage(const char* prog) {
    fprintf(stderr, "usage: %s [FUSE and mount options] rootDir mountPoint\n", prog);
	exit(3);
}

int main(int argc, char *argv[])
{
	if ((argc < 3) || (argv[argc-2][0] == '-') || (argv[argc-1][0] == '-'))
		usage(argv[0]);

	const char* fname = argv[argc-2];
	argv[argc-2] = argv[argc-1];
	argv[argc-1] = NULL;
	argc--;

	adfa_fuse_oper.getattr	= adfa_fuse_getattr,
//	adfa_fuse_oper.readdir	= adfa_fuse_readdir,
	adfa_fuse_oper.open		= adfa_fuse_open,
	adfa_fuse_oper.read		= adfa_fuse_read;

	flog = fopen("fuse.log", "a");
	if (NULL == flog) {
		fprintf(stderr, "fopen(\"%s\", \"a\")=%s\n", "fuse.log", strerror(errno));
		return 3;
	}
	setlinebuf(flog);
	try {
		g_dfa = BaseDFA::load_from(fname);
		return fuse_main(argc, argv, &adfa_fuse_oper, NULL);
	}
	catch (const std::exception& e) {
		fprintf(stderr, "FATAL: execption: %s\n", e.what());
		return 1;
	}
}


DEBUG ?= YES

SUBDIRS			= echo_client echo_server file_client file_server employee_client employee_server async_client async_server

BDB_VER ?= 4.7

all::
	@for subdir in $(SUBDIRS); \
	do \
	    echo "making all in $$subdir"; \
	    ( cd $$subdir && $(MAKE) DEBUG=${DEBUG} BDB_VER=$BDB_VER) || exit 1; \
	done

clean::
	@for subdir in $(SUBDIRS); \
	do \
	    echo "making clean in $$subdir"; \
	    ( cd $$subdir && $(MAKE) BDB_VER=$BDB_VER clean ) || exit 1; \
	done

depend::
	@for subdir in $(SUBDIRS); \
	do \
	    echo "making depend in $$subdir"; \
	    ( cd $$subdir && $(MAKE) BDB_VER=$BDB_VER depend ) || exit 1; \
	done

install::
	@for subdir in $(SUBDIRS); \
	do \
	    echo "making install in $$subdir"; \
	    ( cd $$subdir && $(MAKE) BDB_VER=$BDB_VER install ) || exit 1; \
	done



override CXXFLAGS += -g3 -Wall -Wextra -I../../src
override LDFLAGS = ../../lib/libterark-fsa_all-d.a

ifeq "$(origin LD)" "default"
  LD := ${CXX}
endif

.PHONY:all check
all: load check

load : load.o
	${LD} -o $@ $^ ${LDFLAGS}

sample : sample.o
	${LD} -o $@ $^ ${LDFLAGS}

check: load
	time php big.php > 1
	time php phploadsave.php < 1 > 2
	time ./load < 1 > 3
	diff 1 2
	php mixed_array.php | tee 4 | ./load > 5
	diff 4 5
	php gen.php | tee 6 | ./load > 7
	diff 6 7
	php float.php | tee 8 | ./load > 9
	@echo floating point test error is insignificant, just precision diff
	diff 8 9

clean:
	rm -f load 1 2 3 4 5 6 7 8 9

%.o : %.cpp
	${CXX} -c ${CXXFLAGS} -g3 -o $@ $<



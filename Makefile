CXXFLAGS=-I./tmp-include-dir/include -Iinclude -Iprivate -MMD -std=c++23
CFLAGS=-I./tmp-include-dir/include -Iinclude -Iprivate -MMD 

ifdef RELEASE
CXXFLAGS+=-O2 -g
else
CXXFLAGS+=-O0 -g
endif

LDLIBS=-lscl -ldatalogpp
LDFLAGS=

SRCS=src/main.cc \
	src/confs.cc \
	src/common.cc \
	src/compile.cc \
	src/analysis.cc \
	src/thread_pool.cc \
	src/build.cc \
	src/link.cc \
	src/init.cc \
	src/depfile.cc \
	src/cmdline.cc \
	src/install.cc \
	src/packages.cc \
	src/hooks.cc \
	src/semver.cc \
	src/target.cc \
	src/jayson.cc

CSRCS=csrc/bootstrap_version.c

OBJS=$(SRCS:.cc=.o)
COBJS=$(CSRCS:.c=.o)

init-bootstrap:
	mkdir -p tmp-include-dir/include/
	mkdir -p tmp-include-dir/include/crow.scl
	mkdir -p tmp-include-dir/include/crow.jayson
	mkdir -p tmp-include-dir/include/crow.lexible
	mkdir -p tmp-include-dir/include/crow.datalogpp
	mkdir -p tmp-include-dir/include/crow.terse
	cp /usr/local/include/scl.hh tmp-include-dir/include/crow.scl
	cp /usr/local/include/jayson.hh tmp-include-dir/include/crow.jayson
	cp /usr/local/include/lexible.hh tmp-include-dir/include/crow.lexible
	cp /usr/local/include/datalogpp.hh tmp-include-dir/include/crow.datalogpp
	cp /usr/local/include/terse.hh tmp-include-dir/include/crow.terse

clean:
	rm $(OBJS) 

%.o: %.cc
	$(CXX) $(CXXFLAGS) $^ -c -o $@

%.o: %.c
	$(CC) $(CFLAGS) $^ -c -o $@

bootstrap: $(OBJS) $(COBJS)
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) $(LDLIBS) -o bin/hewg

install:
	sudo cp bin/hewg /usr/local/bin/hewg-bootstrap

uninstall:
	sudo rm -i /usr/local/bin/hewg-bootstrap


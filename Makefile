CXXFLAGS=-I$(HOME)/.hewg/bootstrap/ -Iinclude -Iprivate -MMD -std=c++23
CFLAGS=-I$(HOME)/.hewg/bootstrap/ -Iinclude -Iprivate -MMD 

ifdef RELEASE
CXXFLAGS+=-O2 -g
else
CXXFLAGS+=-O0 -g
endif

LDLIBS=-lscl -ldatalogpp
LDFLAGS=-L$(HOME)/.hewg/bootstrap/

SRCS=src/main.cc \
	src/confs.cc \
	src/common.cc \
	src/compile.cc \
	src/deptree.cc \
	src/srcrelatives.cc \
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

clean:
	rm $(OBJS) 

%.o: %.cc
	$(CXX) $(CXXFLAGS) $^ -c -o $@

%.o: %.c
	$(CC) $(CFLAGS) $^ -c -o $@

bootstrap: $(OBJS) $(COBJS)
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) $(LDLIBS) -o bin/hewg


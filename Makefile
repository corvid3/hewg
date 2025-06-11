ifndef BUILD_LINUX
ifndef BUILD_WINDOWS
$(error BUILD_LINUX or BUILD_WINDOWS must be set!)
endif
endif

ifdef BUILD_LINUX
ifdef BUILD_WINDOWS
$(error BUILD_LINUX and BUILD_WINDOWS may not be set at the same time!)
endif
endif

ifdef BUILD_LINUX
	SRC += 
	CSRC += 

	BINNAME = hewg.out
	INSTALL_NAME = hewg

	LDLIBS += 
endif

ifdef BUILD_WINDOWS
	CXX = x86_64-w64-mingw32-g++
	CC = x86_64-w64-mingw32-gcc

	SRC += 
	CSRC += 

	BINNAME = hewg.exe
	INSTALL_NAME = $(BINNAME)

	# because of mingw32 & linux crossdev,
	# have to statically link the C++ runtime
	LDFLAGS += -static-libgcc -static-libstdc++ -static
	LDLIBS  += 
	LDFLAGS += 
endif


CXXFLAGS=-MMD @compile_flags.txt -std=c++23
CFLAGS=-MMD @ccompile_flags.txt 

ifdef RELEASE
CXXFLAGS+=-O2 -g
else
CXXFLAGS+=-O0 -g
endif

LDLIBS=-lscl
LDFLAGS=

SRCS=src/main.cc src/confs.cc src/compile.cc src/analysis.cc src/thread_pool.cc src/depfile.cc src/cmdline.cc
OBJS=$(SRCS:.cc=.o)
CSRCS=
COBJS=$(CSRCS:.c=.o)

default: hewg

clean:
	rm $(OBJS) 

%.o: %.cc
	$(CXX) $(CXXFLAGS) $^ -c -o $@

%.o: %.c
	$(CC) $(CFLAGS) $^ -c -o $@

hewg: $(OBJS) $(COBJS)
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) $(LDLIBS) -o bin/$(BINNAME)

install:
	cp bin/$(BINNAME) /usr/local/bin/$(INSTALL_NAME)

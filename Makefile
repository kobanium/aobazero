include Makefile.config

ENABLE_GDBINFO ?= 0
ifeq ($(ENABLE_GDBINFO), 1)
	CXXFLAGS += -g
	LDFLAGS  += -g
endif

ENABLE_ASSERT ?= 0
ifeq ($(ENABLE_ASSERT), 1)
	CPPFLAGS += -DDEBUG
else
	CPPFLAGS += -DNDEBUG
endif

USE_OpenMP ?= 0
ifeq ($(USE_OpenMP), 1)
	CXXFLAGS += -fopenmp
	LDFLAGS  += -fopenmp
endif

USE_OpenCL ?= 0
ifeq ($(USE_OpenCL), 1)
	CPPFLAGS += -DUSE_OPENCL
	TARGETS  += bin/ocldevs
	LIB_OpenCL := -lOpenCL
else
	LIB_OpenCL :=
endif

BLAS ?= None
ifeq ($(BLAS), IntelMKL)
	LIB_BLAS := -lmkl_rt
	CPPFLAGS += -DUSE_MKL
	ifdef IntelMKL_INC
		CPPFLAGS += -I$(IntelMKL_INC)
	endif
	ifdef IntelMKL_LIB
		LDFLAGS  += -L$(IntelMKL_LIB) -Wl,-rpath,$(IntelMKL_LIB)
	endif
else ifeq ($(BLAS), OpenBLAS)
	LIB_BLAS := -lopenblas
	CPPFLAGS += -DUSE_OPENBLAS
	ifdef OpenBLAS_INC
		CPPFLAGS += -I$(OpenBLAS_INC)
	endif
	ifdef OpenBLAS_LIB
		LDFLAGS  += -L$(OpenBLAS_LIB) -Wl,-rpath,$(OpenBLAS_LIB)
	endif
endif

CXXFLAGS += -std=c++11 -Wextra -Ofast -march=native -mtune=native
CPPFLAGS += -MMD -MP -Isrc/common -DNDEBUG -DUSE_SSE4
LDFLAGS  += -llzma -lpthread

TARGETS        += bin/aobaz bin/autousi bin/server bin/playshogi bin/crc64 bin/extract bin/net-test bin/gencode
AUTOUSI_OBJS   := src/autousi/autousi.o src/autousi/client.o src/autousi/pipe.o src/common/iobase.o src/common/option.o src/common/jqueue.o src/common/xzi.o src/common/err.o src/common/shogibase.o src/common/osi.o
SERVER_OBJS    := src/server/server.o src/server/listen.o src/server/datakeep.o src/common/iobase.o src/common/xzi.o src/common/jqueue.o src/common/err.o src/common/option.o src/server/logging.o src/common/shogibase.o src/common/osi.o
GENCODE_OBJS   := src/gencode/gencode.o
PLAYSHOGI_OBJS := src/playshogi/playshogi.o src/common/option.o src/common/err.o src/common/iobase.o src/common/xzi.o src/common/shogibase.o src/common/osi.o
CRC64_OBJS     := src/crc64/crc64.o src/common/xzi.o src/common/err.o src/common/iobase.o src/common/osi.o
EXTRACT_OBJS   := src/extract/extract.o src/common/xzi.o src/common/err.o src/common/iobase.o src/common/osi.o
OCLDEVS_OBJS   := src/ocldevs/ocldevs.o src/common/err.o
NET_TEST_OBJS  := src/net-test/net-test.o src/net-test/nnet.o src/net-test/nnet-shogi.o src/net-test/nnet-cpu.o src/common/err.o src/common/iobase.o src/common/shogibase.o src/common/xzi.o src/common/osi.o 
OBJS           := $(AUTOUSI_OBJS) $(SERVER_OBJS) $(GENCODE_OBJS) $(PLAYSHOGI_OBJS) $(CRC64_OBJS) $(EXTRACT_OBJS) $(OCLDEVS_OBJS) $(NET_TEST_OBJS)
INC_OUT        := src/common/tbl_zkey.inc src/common/tbl_board.inc src/common/tbl_sq.inc src/common/tbl_bmap.inc

all: $(TARGETS)

bin/aobaz: src/usi-engine/aobaz
	cp $^ $@

bin/autousi: $(AUTOUSI_OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS)

bin/server: $(SERVER_OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS)

bin/gencode: $(GENCODE_OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS)
	./bin/gencode

bin/playshogi: $(PLAYSHOGI_OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS)

bin/crc64: $(CRC64_OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS)

bin/extract: $(EXTRACT_OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS)

bin/ocldevs: $(OCLDEVS_OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS) $(LIB_OpenCL)

bin/net-test: $(NET_TEST_OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS) $(LIB_BLAS) $(LIB_OpenCL)

.cpp.o:
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<

clean:
	-$(RM) $(TARGETS) $(OBJS) $(OBJS:.o=.d) $(INC_OUT) Makefile~ build_vs.bat~
	cd src/usi-engine; $(MAKE) clean

src/usi-engine/aobaz: FORCE
	cd src/usi-engine; $(MAKE)

$(AUTOUSI_OBJS:.o=.cpp) : bin/gencode
$(SERVER_OBJS:.o=.cpp) : bin/gencode
$(PLAYSHOGI_OBJS:.o=.cpp) : bin/gencode
$(NET_TEST_OBJS:.o=.cpp) : bin/gencode

-include $(OBJS:.o=.d)
FORCE:
.PHONY: all clean FORCE

include Makefile.config

ENABLE_GDBINFO_AOBA ?= 0
ifeq ($(ENABLE_GDBINFO_AOBA), 1)
	CXXFLAGS += -g
	LDFLAGS  += -g
endif

ENABLE_ASSERT_AOBA ?= 0
ifeq ($(ENABLE_ASSERT_AOBA), 1)
	CPPFLAGS += -DDEBUG
else
	CPPFLAGS += -DNDEBUG
endif

USE_OpenCL_AOBA ?= 0
ifeq ($(USE_OpenCL_AOBA), 1)
	CPPFLAGS += -DUSE_OPENCL_AOBA
	TARGETS  += bin/ocldevs
	LIB_OpenCL := -lOpenCL
	CPPFLAGS += -I$(OpenCL_INC_AOBA)
endif

USE_CPUBLAS_AOBA ?= None
ifeq ($(USE_CPUBLAS_AOBA), IntelMKL)
	CPPFLAGS += -DUSE_MKL
	CXXFLAGS += -fopenmp
	LDFLAGS  += -fopenmp
	LIB_BLAS := -lmkl_rt
	ifdef IntelMKL_INC_AOBA
		CPPFLAGS += -I$(IntelMKL_INC_AOBA)
	endif
	ifdef IntelMKL_LIB_AOBA
		LDFLAGS += -L$(IntelMKL_LIB_AOBA)
		LDFLAGS += -Wl,-rpath,$(IntelMKL_LIB_AOBA)
	endif
else ifeq ($(USE_CPUBLAS_AOBA), OpenBLAS)
	CPPFLAGS += -DUSE_OPENBLAS
	CXXFLAGS += -fopenmp
	LDFLAGS  += -fopenmp
	LIB_BLAS := -lopenblas
	ifdef OpenBLAS_INC_AOBA
		CPPFLAGS += -I$(OpenBLAS_INC_AOBA)
	endif
	ifdef OpenBLAS_LIB_AOBA
		LDFLAGS += -L$(OpenBLAS_LIB_AOBA)
		LDFLAGS += -Wl,-rpath,$(OpenBLAS_LIB_AOBA)
	endif
endif

CXXFLAGS += -std=c++11 -Wextra -Ofast -march=native -mtune=native
CPPFLAGS += -MD -MP -Isrc/common -DUSE_SSE4
LDFLAGS  += -llzma -lpthread -lrt

TARGETS         := $(addprefix bin/, aobaz autousi server playshogi crc64 extract net-test gencode ocldevs)
AUTOUSI_BASES   := $(addprefix src/autousi/, autousi client play) $(addprefix src/common/, iobase option jqueue xzi err shogibase osi child nnet nnet-cpu nnet-ocl nnet-srv opencli)
SERVER_BASES    := $(addprefix src/server/, server listen datakeep logging) $(addprefix src/common/, iobase xzi jqueue err option shogibase osi)
GENCODE_BASES   := src/gencode/gencode
PLAYSHOGI_BASES := src/playshogi/playshogi $(addprefix src/common/, option err iobase xzi shogibase osi child nnet nnet-cpu nnet-ocl nnet-srv opencli)
CRC64_BASES     := src/crc64/crc64 $(addprefix src/common/, xzi err iobase osi)
EXTRACT_BASES   := src/extract/extract $(addprefix src/common/, xzi err iobase osi)
OCLDEVS_BASES   := src/ocldevs/ocldevs src/common/err src/common/opencli
NET_TEST_BASES  := src/net-test/net-test $(addprefix src/common/, nnet nnet-cpu nnet-ocl jqueue err iobase shogibase xzi osi option opencli)
BASES           := $(AUTOUSI_BASES) $(SERVER_BASES) $(GENCODE_BASES) $(PLAYSHOGI_BASES) $(CRC64_BASES) $(EXTRACT_BASES) $(OCLDEVS_BASES) $(NET_TEST_BASES)
OBJS            := $(addsuffix .o, $(BASES))
INC_OUT         := $(addprefix src/common/, tbl_zkey.inc tbl_board.inc tbl_sq.inc tbl_bmap.inc)

all: $(TARGETS)

bin/aobaz: src/usi-engine/aobaz
	cp $^ $@

bin/autousi: $(addsuffix .o, $(AUTOUSI_BASES))
	$(CXX) -o $@ $^ $(LDFLAGS) $(LIB_BLAS) $(LIB_OpenCL)

bin/server: $(addsuffix .o, $(SERVER_BASES))
	$(CXX) -o $@ $^ $(LDFLAGS)

bin/gencode: $(addsuffix .o, $(GENCODE_BASES))
	$(CXX) -o $@ $^ $(LDFLAGS)
	./bin/gencode

bin/playshogi: $(addsuffix .o, $(PLAYSHOGI_BASES))
	$(CXX) -o $@ $^ $(LDFLAGS) $(LIB_BLAS) $(LIB_OpenCL)

bin/crc64: $(addsuffix .o, $(CRC64_BASES))
	$(CXX) -o $@ $^ $(LDFLAGS)

bin/extract: $(addsuffix .o, $(EXTRACT_BASES))
	$(CXX) -o $@ $^ $(LDFLAGS)

bin/ocldevs: $(addsuffix .o, $(OCLDEVS_BASES))
	$(CXX) -o $@ $^ $(LDFLAGS) $(LIB_OpenCL)

bin/net-test: $(addsuffix .o, $(NET_TEST_BASES))
	$(CXX) -o $@ $^ $(LDFLAGS) $(LIB_BLAS) $(LIB_OpenCL)

.cpp.o:
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<

clean:
	-$(RM) $(TARGETS) $(OBJS) $(OBJS:.o=.d) $(INC_OUT) Makefile~ build_vs.bat~
	cd src/usi-engine; $(MAKE) clean

src/usi-engine/aobaz: FORCE bin/gencode
	cd src/usi-engine; $(MAKE)

$(addsuffix .cpp, $(AUTOUSI_BASES))   : bin/gencode
$(addsuffix .cpp, $(SERVER_BASES))    : bin/gencode
$(addsuffix .cpp, $(PLAYSHOGI_BASES)) : bin/gencode
$(addsuffix .cpp, $(NET_TEST_BASES))  : bin/gencode

-include $(OBJS:.o=.d)
FORCE:
.PHONY: all clean FORCE

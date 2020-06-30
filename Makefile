CXXFLAGS += -std=c++11 -Wextra -O2 -march=native -mtune=native
CPPFLAGS += -MMD -MP -Isrc/common -DNDEBUG -DUSE_SSE4
#LDFLAGS  += -llzma -lpthread -lOpenCL
LDFLAGS  += -llzma -lpthread -framework OpenCL

TARGETS        := bin/aobaz bin/autousi bin/server bin/gencode bin/playshogi bin/crc64 bin/extract bin/ocldevs bin/net-test
AUTOUSI_OBJS   := src/autousi/autousi.o src/autousi/client.o src/autousi/pipe.o src/common/iobase.o src/common/option.o src/common/jqueue.o src/common/xzi.o src/common/err.o src/common/shogibase.o src/common/osi.o
SERVER_OBJS    := src/server/server.o src/server/listen.o src/server/datakeep.o src/common/iobase.o src/common/xzi.o src/common/jqueue.o src/common/err.o src/common/option.o src/server/logging.o src/common/shogibase.o src/common/osi.o
GENCODE_OBJS   := src/gencode/gencode.o
PLAYSHOGI_OBJS := src/playshogi/playshogi.o src/common/option.o src/common/err.o src/common/iobase.o src/common/xzi.o src/common/shogibase.o src/common/osi.o
CRC64_OBJS     := src/crc64/crc64.o src/common/xzi.o src/common/err.o src/common/iobase.o src/common/osi.o
EXTRACT_OBJS   := src/extract/extract.o src/common/xzi.o src/common/err.o src/common/iobase.o src/common/osi.o
OCLDEVS_OBJS   := src/ocldevs/ocldevs.o src/common/err.o
NET_TEST_OBJS  := src/net-test/net-test.o src/common/err.o src/common/shogibase.o
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
	$(CXX) -o $@ $^ $(LDFLAGS)

bin/net-test: $(NET_TEST_OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS)

.cpp.o:
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<

clean:
	-$(RM) $(TARGETS) $(OBJS) $(OBJS:.o=.d) $(INC_OUT) Makefile~ build_vs.bat~
	cd src/usi-engine; $(MAKE) clean

src/usi-engine/aobaz: FORCE
	cd src/usi-engine; $(MAKE)

src/autousi/pipe.cpp: bin/gencode
src/server/datakeep.cpp: bin/gencode
src/common/shogibase.cpp: bin/gencode
src/net-test/net-test.cpp : bin/gencode
src/playshogi/playshogi.cpp: bin/gencode

-include $(OBJS:.o=.d)
FORCE:
.PHONY: all clean FORCE

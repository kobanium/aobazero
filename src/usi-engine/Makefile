THE_OS := $(shell uname -s)
PROGRAM = aobaz

# isnan() does not work with -ffast-math

default:
	@echo "Detected OS: ${THE_OS}"
	$(MAKE) CXXFLAGS='$(CXXFLAGS) -Wall -Wextra -pipe -O2 -march=native -std=c++14 -DNDEBUG'  \
		LDFLAGS='$(LDFLAGS)' \
		$(PROGRAM)

debug:
	@echo "Detected OS: ${THE_OS}"
	$(MAKE) CC=gcc CXX=g++ \
		CXXFLAGS='$(CXXFLAGS) -Wall -Wextra -pipe -Og -g -std=c++14' \
		LDFLAGS='$(LDFLAGS) -g' \
		$(PROGRAM)

clang:
	@echo "Detected OS: ${THE_OS}"
	$(MAKE) CC=clang-5.0 CXX=clang++-5.0 \
		CXXFLAGS='$(CXXFLAGS) -Wall -Wextra -Wno-missing-braces -O3 -flto -march=native -std=c++14 -DNDEBUG' \
		LDFLAGS='$(LDFLAGS) -flto -fuse-linker-plugin' \
		$(PROGRAM)

#DYNAMIC_LIBS = -lboost_program_options -lpthread -lz
DYNAMIC_LIBS = -lboost_system -lboost_filesystem -lboost_program_options -lpthread -lz
LIBS =


ifeq ($(THE_OS),Linux)
# for Linux with OpenBLAS
	CXXFLAGS += -I/usr/include/openblas
#	CXXFLAGS += -I/usr/include/openblas -I./Eigen
	DYNAMIC_LIBS += -lopenblas
	DYNAMIC_LIBS += -lOpenCL
endif
ifeq ($(THE_OS),Darwin)
# for macOS (comment out the Linux part)
	LIBS += -framework Accelerate
	LIBS += -framework OpenCL
#	CXXFLAGS += -I./Eigen
	CXXFLAGS += -I/System/Library/Frameworks/Accelerate.framework/Versions/Current/Headers
endif

# for MKL instead of OpenBLAS
#DYNAMIC_LIBS += -lmkl_rt
#CXXFLAGS += -I/opt/intel/mkl/include
#LDFLAGS  += -L/opt/intel/mkl/lib/intel64/

#CXXFLAGS += -I.
CXXFLAGS += -I. -DMINIMUM -DTLP -DCSA_LAN -DUSI -DYSS_ZERO -DNO_LOGGING 
#CXXFLAGS += -I. -DMINIMUM -DTLP -DCSA_LAN -DUSI -DYSS_ZERO -DNO_LOGGING -DUSE_CPU_ONLY
CPPFLAGS += -MD -MP


sources = Network.cpp Leela.cpp Utils.cpp Zobrist.cpp GTP.cpp Random.cpp \
	  SMP.cpp \
	  OpenCL.cpp OpenCLScheduler.cpp NNCache.cpp Tuner.cpp CPUPipe.cpp \
      bona/data.cpp bona/main.cpp bona/io.cpp bona/proce.cpp \
      bona/utility.cpp bona/ini.cpp bona/attack.cpp bona/book.cpp \
      bona/makemove.cpp bona/unmake.cpp bona/time.cpp bona/csa.cpp \
      bona/valid.cpp bona/bitop.cpp bona/iterate.cpp bona/searchr.cpp \
      bona/search.cpp bona/quiesrch.cpp bona/evaluate.cpp bona/swap.cpp \
      bona/hash.cpp bona/root.cpp bona/next.cpp bona/movgenex.cpp \
      bona/genevasn.cpp bona/gencap.cpp bona/gennocap.cpp bona/gendrop.cpp \
      bona/mate1ply.cpp bona/rand.cpp bona/learn1.cpp bona/learn2.cpp \
      bona/evaldiff.cpp bona/problem.cpp bona/ponder.cpp bona/thread.cpp \
      bona/sckt.cpp bona/debug.cpp bona/mate3.cpp bona/genchk.cpp \
      bona/phash.cpp bona/dfpn.cpp bona/dfpnhash.cpp bona/ysszero.cpp \
      bona/yss_net.cpp



objects = $(sources:.cpp=.o)
deps = $(sources:%.cpp=%.d)

-include $(deps)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c -o $@ $<

$(PROGRAM): $(objects)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LIBS) $(DYNAMIC_LIBS)

clean:
	-$(RM) $(PROGRAM) $(objects) $(deps)

.PHONY: clean default debug clang

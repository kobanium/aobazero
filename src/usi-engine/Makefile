THE_OS := $(shell uname -s)
PROGRAM = aobaz

# isnan() does not work with -ffast-math

default:
	@echo "Detected OS: ${THE_OS}"
	$(MAKE) CC=gcc CXX=g++ \
		CXXFLAGS='$(CXXFLAGS) -Wall -Wextra -pipe -O3 -g -march=native -std=c++14 -DNDEBUG'  \
		LDFLAGS='$(LDFLAGS) -g' \
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
DYNAMIC_LIBS = -lboost_system -lboost_filesystem -lboost_program_options -lpthread -lz -llzma -lrt
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

# CPU_ONLY = 1 for cpu version.
CPU_ONLY = 0

#CXXFLAGS += -I.
#CXXFLAGS += -I. -DMINIMUM -DTLP -DCSA_LAN -DUSI -DYSS_ZERO -DNO_LOGGING
#CXXFLAGS += -I. -DMINIMUM -DTLP -DCSA_LAN -DUSI -DYSS_ZERO -DNO_LOGGING -DUSE_CPU_ONLY
#CXXFLAGS += -I. -I../batched_play/src/common -DMINIMUM -DTLP -DCSA_LAN -DUSI -DYSS_ZERO -DNO_LOGGING -DUSE_CPU_ONLY -DUSE_OPENBLAS

ifeq ($(CPU_ONLY), 1)
CXXFLAGS += -I. -DMINIMUM -DTLP -DCSA_LAN -DUSI -DYSS_ZERO -DNO_LOGGING -DUSE_CPU_ONLY
else
CXXFLAGS += -I. -DMINIMUM -DTLP -DCSA_LAN -DUSI -DYSS_ZERO -DNO_LOGGING -DUSE_OPENCL
endif


CPPFLAGS += -MD -MP

bases  = Network Leela Utils Zobrist GTP Random SMP OpenCL OpenCLScheduler NNCache Tuner CPUPipe
ifneq ($(CPU_ONLY), 1)
bases += nnet-ipc osi err iobase xzi nnet nnet-ocl opencl shogibase
endif
bases += $(addprefix bona/, data main io proce utility ini attack book makemove unmake time csa valid bitop iterate searchr search quiesrch evaluate swap hash root next movgenex genevasn gencap gennocap gendrop mate1ply rand learn1 learn2 evaldiff problem ponder thread sckt debug mate3 genchk phash dfpn dfpnhash ysszero yss_net)

sources = $(addsuffix .cpp, $(bases))
objects = $(addsuffix .o,   $(bases))
deps    = $(addsuffix .d,   $(bases))

-include $(deps)

%.o : %.cpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c -o $@ $<

%.o : ../common/%.cpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c -o $@ $<

$(PROGRAM): $(objects)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LIBS) $(DYNAMIC_LIBS)

clean:
	-$(RM) $(PROGRAM) $(objects) $(deps)

.PHONY: clean default debug clang

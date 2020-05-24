# ---------------------------

VPATH = common

# program name and object files
PROGRAM = learn
OBJS = yss.o yss_misc.o \
       yss_ki1.o yss_ki2.o \
       yss_dcnn.o \
       iobase.o err.o xzi.o

# define
CC = g++

CFLAGS=-c -Wall -std=c++14 -g3 -Ofast -msse2 -I. -I/usr/local/cuda/include -I/home/yss/caffe/include/ -I/home/yss/caffe/src/ -DYSSFISH -DNDEBUG $(EXCFLAGS)
#CFLAGS=-c -Wall -std=c++14 -g3 -Ofast -msse2 -I. -I/usr/local/cuda/include -I/home/yss/caffe/include/ -I/home/yss/caffe/src/ -DYSSFISH -DNDEBUG $(EXCFLAGS) -DCPU_ONLY -D_DEBUG
LDFLAGS= -L/usr/local/lib -L/usr/local/cuda/lib64 -L/usr/local/cuda/lib -L/home/yss/caffe/build/lib/  -lcaffe -lglog -lgflags -lprotobuf -lleveldb -lboost_system -lm -lboost_thread -lpthread -lstdc++ -llzma


# suffixes rule
.SUFFIXES: .cpp .o

# target         '$^' ... list of files.
$(PROGRAM): $(OBJS)
	$(CC) -o $(PROGRAM) $^ $(LDFLAGS)

# suffixe rule   '$<' ... top file name of list of files.
.cpp.o:
	$(CC) $(CFLAGS) -c $<

# delete target
.PHONY: clean
clean:
	$(RM) $(PROGRAM) $(OBJS)
	$(RM) *.gcda
	$(RM) *.gcno


# program name and object files
PROGRAM = swa
OBJS = swa.o

# define
CC = g++
CFLAGS = -O2 -Wall -std=c++14
LDFLAGS = -lm -lstdc++ -lpthread

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

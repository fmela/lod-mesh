CC := $(shell which clang || which gcc)

INCDIRS =
DBGFLAGS = -g3
OPTFLAGS = -O2
WRNFLAGS = -Wall -W -Wno-deprecated-declarations
CFLAGS := $(WRNFLAGS) $(OPTFLAGS) $(DBGFLAGS) $(INCDIRS:%=I%)

LDDIRS =
LDLIBS = m
GLLIBS =
LDFLAGS = $(LDDIRS:%=-L%) $(LDLIBS:%=-l%) $(GLLIBS:%=-l%) -framework OpenGL -framework GLUT

SRC = $(wildcard *.c)
OBJ = $(SRC:%.c=%.o)

TARGET = lod-mesh

all: $(TARGET)
$(TARGET) : $(OBJ)
	$(CC) -o $(TARGET) $(OBJ) $(LDFLAGS)

%.o : %.c
	$(CC) $(CFLAGS) -c $<

.PHONY : clean
clean :
	rm -f $(TARGET) *.o

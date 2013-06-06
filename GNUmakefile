CC := $(shell which clang || which gcc)

INCDIRS = /usr/X11R6/include
DBGFLAGS = g3
OPTFLAGS = O2
WRNFLAGS = Wall W
CFLAGS  = $(WRNFLAGS) $(OPTFLAGS) $(DBGFLAGS) $(INCDIRS:%=I%)
CFLAGS := $(CFLAGS:%=-%)

LDDIRS = /usr/X11R6/lib
LDLIBS = m
GLLIBS = glut
LDFLAGS = $(LDDIRS:%=-L%) $(LDLIBS:%=-l%) $(GLLIBS:%=-l%) -framework OpenGL

SRC = $(wildcard *.c)
OBJ = $(SRC:%.c=%.o)

TARGET = lod-mesh

all: $(TARGET)
$(TARGET) : $(OBJ)
	$(CC) -o $(TARGET) $(OBJ) $(LDFLAGS)

%.o : %.c
	$(CC) $(CFLAGS) $(INCFLAGS) -c $<

.PHONY : clean
clean :
	rm -f $(TARGET) *.o

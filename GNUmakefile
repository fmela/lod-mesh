CC = gcc
LD = gcc

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

DEPDIR = .depend
DEP = $(SRC:%.c=$(DEPDIR)/%.dep)

TARGET = lod-mesh

all: $(TARGET)
$(TARGET) : $(OBJ)
	$(LD) -o $(TARGET) $(OBJ) $(LDFLAGS)

%.o : %.c
	$(CC) $(CFLAGS) $(INCFLAGS) -c $<

.PHONY : clean
clean :
	rm -f $(TARGET) core $(OBJ)
	rm -rf $(DEPDIR)

.PHONY : depend
depend : $(DEP)
$(DEPDIR)/%.dep : %.c
	@$(SHELL) -c '[ -d $(DEPDIR) ] || mkdir $(DEPDIR)'
	@$(SHELL) -ec 'echo -n "Building dependencies for $< - "; \
		$(CC) -M $(CFLAGS) $< > $@; echo ok.'

-include $(DEP)

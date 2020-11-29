#
# --------------------------------------------------------------------------
#
#      QueueUserEx GNU make Makefile
# 

# If Running MsysDTK
RM	= rm -f
MV	= mv -f
CP	= cp -f

# If not.
#RM	= erase
#MV	= rename
#CP	= copy

AR	= ar

CC	= gcc
#OPT	= -g
OPT	= -O3

LFLAGS		= 

GC_CFLAGS	= 
GCE_CFLAGS	= 

MAKE		= make
CFLAGS	= $(OPT) -I. -D_WIN32_WINNT=0x501 -Wall

DLL_INLINED_OBJS	= \
		pthread.o

# Agregate modules for inlinability
DLL_OBJS	= QueueUserAPCEx.o

DLL		= quserex.dll
LIB		= libquserex.a

$(DLL): $(DLL_OBJS)
	$(CC) $(OPT) -shared -o $(DLL) $(DLL_OBJS) $(LFLAGS)
	dlltool -z quserex.def $(DLL_OBJS)
	dlltool -k --dllname $@ --output-lib $(LIB) --def quserex.def

%.pre: %.c
	$(CC) -E -o $@ $(CFLAGS) $^

%.s: %.c
	$(CC) -c $(CFLAGS) -Wa,-ahl $^ > $@

.SUFFIXES: .dll .c .o

.c.o:;	$(CC) -c -o $@ $(CFLAGS) $<

clean:
	-$(RM) *~
	-$(RM) *.i
	-$(RM) *.o
	-$(RM) *.obj
	-$(RM) *.exe
	-$(RM) quserex.def

realclean: clean
	-$(RM) $(LIB)
	-$(RM) $(DLL)


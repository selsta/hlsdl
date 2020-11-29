
DLL	= quserex.dll

OPTIM	= /O2

CFLAGS	= /W3 /MD /nologo /Yd /Zi /I.

DLL_OBJS	= QueueUserAPCEx.obj

$(DLL): $(DLL_OBJS)
	cl /LD /Zi /nologo $(DLL_OBJS) \
		/link /nodefaultlib:libcmt /implib:$*.lib \
		msvcrt.lib /out:$@

realclean: clean
	if exist *.dll del *.dll
	if exist *.lib del *.lib

clean:
	if exist *.obj del *.obj
	if exist *.ilk del *.ilk
	if exist *.pdb del *.pdb
	if exist *.exp del *.exp
	if exist *.o del *.o
	if exist *.i del *.i

.c.obj:
	cl $(OPTIM) $(CFLAGS) -c $<

.c.i:
	cl /P /O2 /Ob1 $<

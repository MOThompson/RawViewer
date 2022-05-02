#################################################################
# REMEMBER ... May have to undo a "cl64.bat" command
#################################################################
CFLAGS = /nologo /W3 /WX- /O2 /Ob1 /GF /MD /GS /Gy /fp:precise /Zc:forScope /Gd
LFLAGS = /nologo /subsystem:windows
RFLAGS = /nologo

SYSLIBS = user32.lib gdi32.lib comdlg32.lib comctl32.lib advapi32.lib vfw32.lib shell32.lib

# ----------------------------------------------------------------------------
# Inference rules
# ----------------------------------------------------------------------------
.SUFFIXES:
.SUFFIXES: .obj .c

.c.obj:
	$(CC) $(COPTS) $(SYS) -c -Fo$@ $<

###########################################################################
OBJS = RawViewer.obj win32ex.obj graph.obj timer.obj
###########################################################################

ALL: info.exe RawViewer.exe

INSTALL: c:\local.nt\bin\RawViewer.exe z:\lab\exes\RawViewer.exe

CLEAN: 
	rm *.exe *.obj *.res
	rm win32ex.c win32ex.h
	rm graph.c graph.h 
	rm camera.h tl.h

# ----------------------------------------------------------- #
# ------------------- Basic Library Build ------------------- #
# ----------------------------------------------------------- #
info.exe : info.obj
	$(CC) -Fe$@ $** $(SYSLIBS)

RawViewer.exe : $(OBJS) RawViewer.res
	$(CC) -Fe$@ $** $(SYSLIBS)

RawViewer.res : RawViewer.rc resource.h
	rc $(RFLAGS) RawViewer.rc

# Install locations
c:\local.nt\bin\RawViewer.exe : RawViewer.exe
	cp $** $@

z:\lab\exes\RawViewer.exe : RawViewer.exe
	cp $** $@


# ---------------------------------------------------------------------------
# Support modules 
# Explicit compile is unnecessary as will use $(CFLAGS) since that is default
# ---------------------------------------------------------------------------
camera.h : ..\ZooCam\camera.h
	cp $** $@
tl.h : ..\ZooCam\tl.h
	cp $** $@

win32ex.h : \code\Window_Classes\win32ex\win32ex.h
	copy $** $@

win32ex.c : \code\Window_Classes\win32ex\win32ex.c
	copy $** $@

win32ex.obj : win32ex.c win32ex.h

graph.h : \code\Window_Classes\graph\graph.h
	copy $** $@

graph.c : \code\Window_Classes\graph\graph.c
	copy $** $@

graph.obj : graph.c graph.h

timer.h : \code\lab\TimeFncs\timer.h
	copy $** $@

timer.c : \code\lab\TimeFncs\timer.c
	copy $** $@

timer.obj : timer.c timer.h

# ----------------------------------------------------------- #
# -------------- Include File Dependencies ------------------ #
# ----------------------------------------------------------- #
RawViewer.obj : resource.h win32ex.h graph.h timer.h camera.h tl.h
info.obj : camera.h tl.h

# Make for run of the mill linked files
.c}.obj:
	$(CC) $(CFLAGS) -c -Fo$@ $<

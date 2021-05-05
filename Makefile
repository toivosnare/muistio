SDK_PATH = "C:\Program Files (x86)\Windows Kits\10"
SDK_VERSION = 10.0.19041.0
SDK_INC = $(SDK_PATH)\Include\$(SDK_VERSION)
SDK_LIB = $(SDK_PATH)\Lib\$(SDK_VERSION)
SDK_BIN = $(SDK_PATH)\bin\$(SDK_VERSION)
VS = "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Tools\MSVC\14.23.28105"

INC_VS = $(VS)\include
INC_SDK_SH = $(SDK_INC)\shared
INC_SDK_UM = $(SDK_INC)\um
INC_SDK_UC = $(SDK_INC)\ucrt

LIB_VS = $(VS)\lib\x86
LIB_SDK_UM = $(SDK_LIB)\um\x86
LIB_SDK_UC = $(SDK_LIB)\ucrt\x86

CXX = $(VS)\bin\Hostx86\x86\cl.exe
RC = $(SDK_BIN)\x86\rc.exe
LD = $(VS)\bin\Hostx86\x86\link.exe

CXXFLAGS = /c /std:c++17 /nologo
RCFLAGS = /nologo
LDFLAGS = /nologo

LIBS = user32.lib Comdlg32.lib Comctl32.lib
OBJS = main.obj fileio.obj encoding.res

all: $(OBJS)
	$(LD) $(LDFLAGS) /LIBPATH:$(LIB_VS) /LIBPATH:$(LIB_SDK_UM) /LIBPATH:$(LIB_SDK_UC) $(OBJS) $(LIBS)

%.obj: %.cpp
	$(CXX) $(CXXFLAGS) /I $(INC_VS) /I $(INC_SDK_SH) /I $(INC_SDK_UM) /I $(INC_SDK_UC) $<

%.res: %.rc
	$(RC) $(RCFLAGS) /I $(INC_SDK_SH) /I $(INC_SDK_UM) $<

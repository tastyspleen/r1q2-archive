# Microsoft Developer Studio Project File - Name="quake2" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=quake2 - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "quake2.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "quake2.mak" CFG="quake2 - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "quake2 - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "quake2 - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE "quake2 - Win32 Dedicated Only" (based on "Win32 (x86) Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "quake2 - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir ".\Release"
# PROP BASE Intermediate_Dir ".\Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "./build/binaries/release"
# PROP Intermediate_Dir "./build/temp/r1q2/release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /c
# ADD CPP /nologo /G6 /MD /W3 /GX /O2 /Ob2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /FD /c
# SUBTRACT CPP /Fr
# ADD BASE MTL /nologo /D "NDEBUG" /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /machine:I386
# ADD LINK32 winmm.lib wsock32.lib kernel32.lib user32.lib gdi32.lib zlib.lib advapi32.lib dxguid.lib dinput8.lib /nologo /subsystem:windows /pdb:none /machine:I386 /out:"./build/binaries/release/r1q2.exe"
# SUBTRACT LINK32 /debug

!ELSEIF  "$(CFG)" == "quake2 - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir ".\Debug"
# PROP BASE Intermediate_Dir ".\Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "./build/binaries/debug"
# PROP Intermediate_Dir "./build/temp/r1q2/debug"
# PROP Ignore_Export_Lib 1
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /c
# ADD CPP /nologo /G5 /MD /W3 /Gm /Gi /GX /Zi /Od /Gy /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /FD /c
# SUBTRACT CPP /Fr /YX
# ADD BASE MTL /nologo /D "_DEBUG" /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /debug /machine:I386
# ADD LINK32 winmm.lib wsock32.lib kernel32.lib user32.lib zlib.lib advapi32.lib dxguid.lib dinput8.lib /nologo /subsystem:windows /pdb:none /map /machine:I386 /out:"./build/binaries/debug/r1q2.exe"
# SUBTRACT LINK32 /profile /debug /nodefaultlib

!ELSEIF  "$(CFG)" == "quake2 - Win32 Dedicated Only"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "quake2___Win32_Dedicated_Only"
# PROP BASE Intermediate_Dir "quake2___Win32_Dedicated_Only"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "./build/binaries/release"
# PROP Intermediate_Dir "./build/temp/dedicated"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /G6 /MT /W3 /GX /Ot /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /FD /O3 /Qsox- /c
# ADD CPP /nologo /G6 /MD /W3 /Ox /Ot /Og /Oi /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "DEDICATED_ONLY" /FR /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 winmm.lib wsock32.lib kernel32.lib user32.lib gdi32.lib zlib.lib /nologo /subsystem:windows /machine:I386
# SUBTRACT BASE LINK32 /incremental:yes /debug /nodefaultlib
# ADD LINK32 winmm.lib wsock32.lib kernel32.lib user32.lib gdi32.lib zlib.lib advapi32.lib /nologo /subsystem:windows /pdb:none /machine:I386 /out:"./build/binaries/release/dedicated.exe" /libpath:"m:\quake2\lib"
# SUBTRACT LINK32 /debug /nodefaultlib

!ENDIF 

# Begin Target

# Name "quake2 - Win32 Release"
# Name "quake2 - Win32 Debug"
# Name "quake2 - Win32 Dedicated Only"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;hpj;bat;for;f90"
# Begin Source File

SOURCE=.\win32\cd_win.c

!IF  "$(CFG)" == "quake2 - Win32 Release"

!ELSEIF  "$(CFG)" == "quake2 - Win32 Debug"

!ELSEIF  "$(CFG)" == "quake2 - Win32 Dedicated Only"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\client\cl_cin.c

!IF  "$(CFG)" == "quake2 - Win32 Release"

!ELSEIF  "$(CFG)" == "quake2 - Win32 Debug"

!ELSEIF  "$(CFG)" == "quake2 - Win32 Dedicated Only"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\client\cl_dll.c
# End Source File
# Begin Source File

SOURCE=.\client\cl_ents.c

!IF  "$(CFG)" == "quake2 - Win32 Release"

!ELSEIF  "$(CFG)" == "quake2 - Win32 Debug"

!ELSEIF  "$(CFG)" == "quake2 - Win32 Dedicated Only"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\client\cl_fx.c

!IF  "$(CFG)" == "quake2 - Win32 Release"

!ELSEIF  "$(CFG)" == "quake2 - Win32 Debug"

!ELSEIF  "$(CFG)" == "quake2 - Win32 Dedicated Only"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\client\cl_input.c

!IF  "$(CFG)" == "quake2 - Win32 Release"

!ELSEIF  "$(CFG)" == "quake2 - Win32 Debug"

!ELSEIF  "$(CFG)" == "quake2 - Win32 Dedicated Only"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\client\cl_inv.c

!IF  "$(CFG)" == "quake2 - Win32 Release"

!ELSEIF  "$(CFG)" == "quake2 - Win32 Debug"

!ELSEIF  "$(CFG)" == "quake2 - Win32 Dedicated Only"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\client\cl_main.c

!IF  "$(CFG)" == "quake2 - Win32 Release"

!ELSEIF  "$(CFG)" == "quake2 - Win32 Debug"

!ELSEIF  "$(CFG)" == "quake2 - Win32 Dedicated Only"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\client\cl_newfx.c

!IF  "$(CFG)" == "quake2 - Win32 Release"

!ELSEIF  "$(CFG)" == "quake2 - Win32 Debug"

!ELSEIF  "$(CFG)" == "quake2 - Win32 Dedicated Only"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\client\cl_parse.c

!IF  "$(CFG)" == "quake2 - Win32 Release"

!ELSEIF  "$(CFG)" == "quake2 - Win32 Debug"

!ELSEIF  "$(CFG)" == "quake2 - Win32 Dedicated Only"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\client\cl_pred.c

!IF  "$(CFG)" == "quake2 - Win32 Release"

!ELSEIF  "$(CFG)" == "quake2 - Win32 Debug"

!ELSEIF  "$(CFG)" == "quake2 - Win32 Dedicated Only"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\client\cl_scrn.c

!IF  "$(CFG)" == "quake2 - Win32 Release"

!ELSEIF  "$(CFG)" == "quake2 - Win32 Debug"

!ELSEIF  "$(CFG)" == "quake2 - Win32 Dedicated Only"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\client\cl_tent.c

!IF  "$(CFG)" == "quake2 - Win32 Release"

!ELSEIF  "$(CFG)" == "quake2 - Win32 Debug"

!ELSEIF  "$(CFG)" == "quake2 - Win32 Dedicated Only"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\client\cl_view.c

!IF  "$(CFG)" == "quake2 - Win32 Release"

!ELSEIF  "$(CFG)" == "quake2 - Win32 Debug"

!ELSEIF  "$(CFG)" == "quake2 - Win32 Dedicated Only"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\qcommon\cmd.c
# End Source File
# Begin Source File

SOURCE=.\qcommon\cmodel.c
# End Source File
# Begin Source File

SOURCE=.\qcommon\common.c
# End Source File
# Begin Source File

SOURCE=.\win32\conproc.c
# End Source File
# Begin Source File

SOURCE=.\client\console.c

!IF  "$(CFG)" == "quake2 - Win32 Release"

!ELSEIF  "$(CFG)" == "quake2 - Win32 Debug"

!ELSEIF  "$(CFG)" == "quake2 - Win32 Dedicated Only"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\qcommon\crc.c
# End Source File
# Begin Source File

SOURCE=.\qcommon\cvar.c
# End Source File
# Begin Source File

SOURCE=.\qcommon\files.c
# End Source File
# Begin Source File

SOURCE=.\win32\in_win.c

!IF  "$(CFG)" == "quake2 - Win32 Release"

!ELSEIF  "$(CFG)" == "quake2 - Win32 Debug"

!ELSEIF  "$(CFG)" == "quake2 - Win32 Dedicated Only"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\client\keys.c

!IF  "$(CFG)" == "quake2 - Win32 Release"

!ELSEIF  "$(CFG)" == "quake2 - Win32 Debug"

!ELSEIF  "$(CFG)" == "quake2 - Win32 Dedicated Only"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\client\le_physics.c

!IF  "$(CFG)" == "quake2 - Win32 Release"

!ELSEIF  "$(CFG)" == "quake2 - Win32 Debug"

!ELSEIF  "$(CFG)" == "quake2 - Win32 Dedicated Only"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\client\le_util.c

!IF  "$(CFG)" == "quake2 - Win32 Release"

!ELSEIF  "$(CFG)" == "quake2 - Win32 Debug"

!ELSEIF  "$(CFG)" == "quake2 - Win32 Dedicated Only"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\game\m_flash.c

!IF  "$(CFG)" == "quake2 - Win32 Release"

!ELSEIF  "$(CFG)" == "quake2 - Win32 Debug"

!ELSEIF  "$(CFG)" == "quake2 - Win32 Dedicated Only"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\qcommon\md4.c
# End Source File
# Begin Source File

SOURCE=.\client\menu.c

!IF  "$(CFG)" == "quake2 - Win32 Release"

!ELSEIF  "$(CFG)" == "quake2 - Win32 Debug"

!ELSEIF  "$(CFG)" == "quake2 - Win32 Dedicated Only"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\qcommon\mersennetwister.c
# End Source File
# Begin Source File

SOURCE=.\qcommon\net_chan.c
# End Source File
# Begin Source File

SOURCE=.\win32\net_wins.c
# End Source File
# Begin Source File

SOURCE=.\qcommon\pmove.c
# End Source File
# Begin Source File

SOURCE=.\game\q_shared.c
# End Source File
# Begin Source File

SOURCE=.\win32\q_shwin.c
# End Source File
# Begin Source File

SOURCE=.\client\qmenu.c

!IF  "$(CFG)" == "quake2 - Win32 Release"

!ELSEIF  "$(CFG)" == "quake2 - Win32 Debug"

!ELSEIF  "$(CFG)" == "quake2 - Win32 Dedicated Only"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\qcommon\redblack.c
# End Source File
# Begin Source File

SOURCE=.\client\snd_dma.c

!IF  "$(CFG)" == "quake2 - Win32 Release"

!ELSEIF  "$(CFG)" == "quake2 - Win32 Debug"

!ELSEIF  "$(CFG)" == "quake2 - Win32 Dedicated Only"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\client\snd_mem.c

!IF  "$(CFG)" == "quake2 - Win32 Release"

!ELSEIF  "$(CFG)" == "quake2 - Win32 Debug"

!ELSEIF  "$(CFG)" == "quake2 - Win32 Dedicated Only"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\client\snd_mix.c

!IF  "$(CFG)" == "quake2 - Win32 Release"

!ELSEIF  "$(CFG)" == "quake2 - Win32 Debug"

!ELSEIF  "$(CFG)" == "quake2 - Win32 Dedicated Only"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\client\alw_win.c

!IF  "$(CFG)" == "quake2 - Win32 Release"

!ELSEIF  "$(CFG)" == "quake2 - Win32 Debug"

!ELSEIF  "$(CFG)" == "quake2 - Win32 Dedicated Only"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\client\qal_win.c

!IF  "$(CFG)" == "quake2 - Win32 Release"

!ELSEIF  "$(CFG)" == "quake2 - Win32 Debug"

!ELSEIF  "$(CFG)" == "quake2 - Win32 Dedicated Only"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\win32\snd_win.c

!IF  "$(CFG)" == "quake2 - Win32 Release"

!ELSEIF  "$(CFG)" == "quake2 - Win32 Debug"

!ELSEIF  "$(CFG)" == "quake2 - Win32 Dedicated Only"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\server\sv_ccmds.c
# End Source File
# Begin Source File

SOURCE=.\server\sv_ents.c
# End Source File
# Begin Source File

SOURCE=.\server\sv_game.c
# End Source File
# Begin Source File

SOURCE=.\server\sv_init.c
# End Source File
# Begin Source File

SOURCE=.\server\sv_main.c
# End Source File
# Begin Source File

SOURCE=.\server\sv_send.c
# End Source File
# Begin Source File

SOURCE=.\server\sv_user.c
# End Source File
# Begin Source File

SOURCE=.\server\sv_world.c
# End Source File
# Begin Source File

SOURCE=.\win32\sys_win.c
# End Source File
# Begin Source File

SOURCE=.\win32\vid_dll.c

!IF  "$(CFG)" == "quake2 - Win32 Release"

!ELSEIF  "$(CFG)" == "quake2 - Win32 Debug"

!ELSEIF  "$(CFG)" == "quake2 - Win32 Dedicated Only"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\win32\vid_menu.c

!IF  "$(CFG)" == "quake2 - Win32 Release"

!ELSEIF  "$(CFG)" == "quake2 - Win32 Debug"

!ELSEIF  "$(CFG)" == "quake2 - Win32 Dedicated Only"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl;fi;fd"
# Begin Source File

SOURCE=.\client\anorms.h
# End Source File
# Begin Source File

SOURCE=.\qcommon\bspfile.h
# End Source File
# Begin Source File

SOURCE=.\client\cdaudio.h
# End Source File
# Begin Source File

SOURCE=.\client\client.h
# End Source File
# Begin Source File

SOURCE=.\win32\conproc.h
# End Source File
# Begin Source File

SOURCE=.\client\console.h
# End Source File
# Begin Source File

SOURCE=.\game\game.h
# End Source File
# Begin Source File

SOURCE=.\client\input.h
# End Source File
# Begin Source File

SOURCE=.\client\keys.h
# End Source File
# Begin Source File

SOURCE=.\game\q_shared.h
# End Source File
# Begin Source File

SOURCE=.\qcommon\qcommon.h
# End Source File
# Begin Source File

SOURCE=.\qcommon\qfiles.h
# End Source File
# Begin Source File

SOURCE=.\client\qmenu.h
# End Source File
# Begin Source File

SOURCE=.\client\ref.h
# End Source File
# Begin Source File

SOURCE=.\client\screen.h
# End Source File
# Begin Source File

SOURCE=.\server\server.h
# End Source File
# Begin Source File

SOURCE=.\client\snd_loc.h
# End Source File
# Begin Source File

SOURCE=.\client\sound.h
# End Source File
# Begin Source File

SOURCE=.\client\vid.h
# End Source File
# Begin Source File

SOURCE=.\win32\winquake.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;cnt;rtf;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\win32\q2.ico

!IF  "$(CFG)" == "quake2 - Win32 Release"

!ELSEIF  "$(CFG)" == "quake2 - Win32 Debug"

!ELSEIF  "$(CFG)" == "quake2 - Win32 Dedicated Only"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\win32\q2.rc

!IF  "$(CFG)" == "quake2 - Win32 Release"

!ELSEIF  "$(CFG)" == "quake2 - Win32 Debug"

!ELSEIF  "$(CFG)" == "quake2 - Win32 Dedicated Only"

!ENDIF 

# End Source File
# End Group
# End Target
# End Project

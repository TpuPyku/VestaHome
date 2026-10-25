@echo off
rem Builds VestaStub.exe (Windows CE, ARMv4: goes in place of G3NavHMI.exe) and VestaStubPC.exe (desktop x86, for
rem testing) without Visual Studio 2005: the ARM part with eVC4's ARM toolchain (clarm 12.20 + link 6.24) and the
rem Pocket PC 2003 SDK, the PC part with the Visual Studio 2019 Build Tools. build.bat (VS2005) stays the reference
rem build and gives the same ARM exe (ARM (CE), subsystem 4.20, GUI, imports only COREDLL.dll); both write
rem ..\VestaStub\ but keep their objects apart (obj\evc4_arm, obj\vs2019_pc, build.bat's obj\arm, obj\pc).
rem Toolchains, all overridable from the environment:
rem   EVC4   - the folder with bin\clarm.exe, bin\link.exe (taken from the eVC4 4.0 ISO with SP2, no install:
rem            robocopy "<ISO>\eVC4SP2\EVC\WCE420\bin" C:\eVC4\bin /E; the IDE itself is not needed);
rem   SDK    - the Pocket PC 2003 SDK (unpacked without admin rights: msiexec /a <msi> TARGETDIR=C:\PPC2003SDK);
rem   VS2019 - the Build Tools, called through VC\Auxiliary\Build\vcvars32.bat.
rem A missing toolchain skips that half with a NOTE; nothing built at all is an error.
rem eVC4's compiler is old C++: it reads the source in the ANSI code page only (no UTF-8 BOM), so the Cyrillic
rem strings are converted into obj\evc4_arm\stub_evc4.cpp (CP1251, what the unit's Russian locale expects), and it
rem has no conforming for-scope (/Zc:forScope came with VS2005), hence stub.cpp names the counters of the second
rem loops in a function differently from the first.
setlocal

if "%EVC4%"=="" set EVC4=C:\eVC4
if "%SDK%"=="" set SDK=C:\PPC2003SDK\POCKET PC 2003
if "%VS2019%"=="" set VS2019=C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools
set OUT=..\VestaStub
set BUILT=0

cd /d "%~dp0"
if not exist obj\evc4_arm mkdir obj\evc4_arm
if not exist obj\vs2019_pc mkdir obj\vs2019_pc
if not exist "%OUT%" mkdir "%OUT%"

echo === ARM (Windows CE)
if not exist "%EVC4%\bin\clarm.exe" goto no_arm
if not exist "%SDK%\Lib\Armv4\coredll.lib" goto no_arm
powershell -NoProfile -Command "$t = [IO.File]::ReadAllText('%~dp0stub.cpp', [Text.Encoding]::UTF8); [IO.File]::WriteAllText('%~dp0obj\evc4_arm\stub_evc4.cpp', $t, [Text.Encoding]::GetEncoding(1251))" || goto fail
"%EVC4%\bin\clarm.exe" /nologo /O2 /W3 /TP /c obj\evc4_arm\stub_evc4.cpp /Foobj\evc4_arm\ ^
  /DUNICODE /D_UNICODE /DUNDER_CE=0x420 /D_WIN32_WCE=0x420 /DWINCE /DARM /D_ARM_ /DARMV4 /DNDEBUG ^
  /I"%SDK%\Include\Armv4" || goto fail
"%EVC4%\bin\link.exe" /nologo /SUBSYSTEM:WINDOWSCE,4.20 /MACHINE:ARM /OUT:"%OUT%\VestaStub.exe" ^
  obj\evc4_arm\stub_evc4.obj /LIBPATH:"%SDK%\Lib\Armv4" coredll.lib corelibc.lib ^
  /NODEFAULTLIB:oldnames.lib /NODEFAULTLIB:advapi32.lib || goto fail
set BUILT=1
goto pc
:no_arm
echo NOTE: no eVC4 ARM compiler or Pocket PC 2003 SDK ("%EVC4%", "%SDK%") - the ARM build is skipped

:pc
echo === x86 (desktop test build)
if not exist "%VS2019%\VC\Auxiliary\Build\vcvars32.bat" goto no_pc
call "%VS2019%\VC\Auxiliary\Build\vcvars32.bat" >nul || goto fail
cl /nologo /O2 /W3 /MT /TP /c stub.cpp /Foobj\vs2019_pc\ ^
  /DUNICODE /D_UNICODE /DWIN32 /DNDEBUG /D_CRT_SECURE_NO_WARNINGS || goto fail
link /nologo /SUBSYSTEM:WINDOWS /OUT:"%OUT%\VestaStubPC.exe" obj\vs2019_pc\stub.obj ^
  user32.lib kernel32.lib || goto fail
set BUILT=1
goto done
:no_pc
echo NOTE: no Visual Studio 2019 Build Tools ("%VS2019%") - the PC build is skipped

:done
if "%BUILT%"=="0" goto nothing
echo === OK
exit /b 0
:nothing
echo === NOTHING BUILT
exit /b 1
:fail
echo === BUILD FAILED
exit /b 1

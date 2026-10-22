@echo off
rem Builds VestaNMEA.dll (Windows CE, ARMv4: VestaHome's NMEA port COM6, a user-mode stream driver) and runs its
rem desktop test (test_vnmea.cpp: the port's logic over a made-up "VestaGPS" shared memory) with Visual Studio 2005.
setlocal

if "%VS8%"=="" set VS8=C:\Program Files (x86)\Microsoft Visual Studio 8
set VC=%VS8%\VC
set SDK=%VS8%\SmartDevices\SDK\PocketPC2003
set PATH=%VS8%\Common7\IDE;%VC%\bin;%PATH%
set OUT=..\VestaNMEA
rem VestaCore (vgps.h: the shared memory's layout): the working copy next to this repo, else the submodule core\
set CORE=%~dp0..\..\..\VestaCore
if not exist "%CORE%\vgps.h" set CORE=%~dp0..\..\core
if not exist "%CORE%\vgps.h" goto nocore

cd /d "%~dp0"
if not exist obj\arm mkdir obj\arm
if not exist obj\pc  mkdir obj\pc
if not exist "%OUT%" mkdir "%OUT%"

echo === ARM (Windows CE)
"%VC%\ce\bin\x86_arm\cl.exe" /nologo /O2 /W3 /GS- /TP /c vnmea.cpp /Foobj\arm\ ^
  /DUNICODE /D_UNICODE /DUNDER_CE=0x420 /D_WIN32_WCE=0x420 /DWINCE /DARM /D_ARM_ /DARMV4 /DNDEBUG ^
  /I"%SDK%\Include" /I"%VC%\ce\include" /I"%CORE%"
if errorlevel 1 goto fail
"%VC%\ce\bin\x86_arm\link.exe" /nologo /DLL /SUBSYSTEM:WINDOWSCE,4.20 /MACHINE:ARM ^
  /OUT:"%OUT%\VestaNMEA.dll" /IMPLIB:obj\arm\VestaNMEA.lib /DEF:vnmea.def obj\arm\vnmea.obj ^
  /LIBPATH:"%SDK%\Lib\armv4" /LIBPATH:"%VC%\ce\lib\armv4" ^
  coredll.lib corelibc.lib /NODEFAULTLIB:oldnames.lib
if errorlevel 1 goto fail

echo === x86 test
"%VC%\bin\cl.exe" /nologo /O2 /W3 /MT /TP test_vnmea.cpp /Foobj\pc\ /Feobj\pc\test_vnmea.exe ^
  /DUNICODE /D_UNICODE /DWIN32 /DNDEBUG /D_CRT_SECURE_NO_WARNINGS ^
  /I"%VC%\include" /I"%VC%\PlatformSDK\Include" /I"%CORE%" ^
  /link /LIBPATH:"%VC%\lib" /LIBPATH:"%VC%\PlatformSDK\Lib" kernel32.lib
if errorlevel 1 goto fail
obj\pc\test_vnmea.exe
if errorlevel 1 goto fail

echo === OK
exit /b 0
:nocore
echo VestaCore not found (..\..\..\VestaCore or ..\..\core)
:fail
echo === BUILD FAILED
exit /b 1

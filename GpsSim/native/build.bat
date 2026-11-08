@echo off
rem Builds GpsSim.exe (Windows CE, ARMv4) and GpsSimPC.exe (desktop x86, for testing) into ..\GpsSim with
rem Visual Studio 2005 (Smart Device support + Pocket PC 2003 SDK). The drive: route.inc (tools\make_route.py).
setlocal

if "%VS8%"=="" set VS8=C:\Program Files (x86)\Microsoft Visual Studio 8
set VC=%VS8%\VC
set SDK=%VS8%\SmartDevices\SDK\PocketPC2003
set PATH=%VS8%\Common7\IDE;%VC%\bin;%PATH%
set OUT=..\GpsSim
rem VestaCore: a working copy of the VestaCore repo next to this repo if there is one, otherwise the submodule core\
set CORE=%~dp0..\..\..\VestaCore
if not exist "%CORE%\vgps.cpp" set CORE=%~dp0..\..\core
if not exist "%CORE%\vgps.cpp" (echo === no VestaCore & exit /b 1)
set SRC=gpssim.cpp simnmea.cpp "%CORE%\vgps.cpp"

cd /d "%~dp0"
if not exist obj\arm mkdir obj\arm
if not exist obj\pc  mkdir obj\pc
if not exist "%OUT%" mkdir "%OUT%"

echo === ARM (Windows CE)
"%VC%\ce\bin\x86_arm\cl.exe" /nologo /O2 /W3 /GS- /TP /c %SRC% /Foobj\arm\ ^
  /DUNICODE /D_UNICODE /DUNDER_CE=0x420 /D_WIN32_WCE=0x420 /DWINCE /DARM /D_ARM_ /DARMV4 /DNDEBUG ^
  /I"%SDK%\Include" /I"%VC%\ce\include" /I"%CORE%"
if errorlevel 1 goto fail
"%VC%\ce\bin\x86_arm\link.exe" /nologo /SUBSYSTEM:WINDOWSCE,4.20 /MACHINE:ARM /OUT:"%OUT%\GpsSim.exe" ^
  obj\arm\*.obj /LIBPATH:"%SDK%\Lib\armv4" /LIBPATH:"%VC%\ce\lib\armv4" ^
  coredll.lib corelibc.lib /NODEFAULTLIB:oldnames.lib
if errorlevel 1 goto fail

echo === x86 (desktop test build)
"%VC%\bin\cl.exe" /nologo /O2 /W3 /MT /TP /c %SRC% /Foobj\pc\ ^
  /DUNICODE /D_UNICODE /DWIN32 /DNDEBUG /D_CRT_SECURE_NO_WARNINGS ^
  /I"%VC%\include" /I"%VC%\PlatformSDK\Include" /I"%CORE%"
if errorlevel 1 goto fail
"%VC%\bin\link.exe" /nologo /SUBSYSTEM:WINDOWS /OUT:"%OUT%\GpsSimPC.exe" obj\pc\*.obj ^
  /LIBPATH:"%VC%\lib" /LIBPATH:"%VC%\PlatformSDK\Lib" user32.lib gdi32.lib kernel32.lib
if errorlevel 1 goto fail

rem the same program as GpsSimOSM: it starts OSMapsCE too (a device that starts one program: a Shturmann)
copy /y "%OUT%\GpsSim.exe" "%OUT%\GpsSimOSM.exe" >nul
copy /y "%OUT%\GpsSimPC.exe" "%OUT%\GpsSimOSMPC.exe" >nul

echo === OK
exit /b 0
:fail
echo === BUILD FAILED
exit /b 1

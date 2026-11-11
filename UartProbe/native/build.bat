@echo off
rem Builds UartProbe.exe (Windows CE, ARMv4) into ..\UartProbe with Visual Studio 2005 (Smart Device support +
rem Pocket PC 2003 SDK); first its frame test (test_mcuframe.bat).
setlocal

if "%VS8%"=="" set VS8=C:\Program Files (x86)\Microsoft Visual Studio 8
set VC=%VS8%\VC
set SDK=%VS8%\SmartDevices\SDK\PocketPC2003
set PATH=%VS8%\Common7\IDE;%VC%\bin;%PATH%
set OUT=..\UartProbe
rem VestaCore: a working copy of the VestaCore repo next to this repo if there is one, otherwise the submodule core\
set CORE=%~dp0..\..\..\VestaCore
if not exist "%CORE%\vlogdir.cpp" set CORE=%~dp0..\..\core
if not exist "%CORE%\vlogdir.cpp" (echo === no VestaCore & exit /b 1)
set SRC=probe.cpp mcuframe.cpp "%CORE%\vlogdir.cpp"

cd /d "%~dp0"
call .\test_mcuframe.bat || (echo === TEST FAILED & exit /b 1)
if not exist obj\arm mkdir obj\arm
if not exist "%OUT%" mkdir "%OUT%"

echo === ARM (Windows CE)
"%VC%\ce\bin\x86_arm\cl.exe" /nologo /O2 /W3 /GS- /TP /c %SRC% /Foobj\arm\ ^
  /DUNICODE /D_UNICODE /DUNDER_CE=0x420 /D_WIN32_WCE=0x420 /DWINCE /DARM /D_ARM_ /DARMV4 /DNDEBUG ^
  /I"%SDK%\Include" /I"%VC%\ce\include" /I"%CORE%"
if errorlevel 1 goto fail
"%VC%\ce\bin\x86_arm\link.exe" /nologo /SUBSYSTEM:WINDOWSCE,4.20 /MACHINE:ARM /OUT:"%OUT%\UartProbe.exe" ^
  obj\arm\*.obj /LIBPATH:"%SDK%\Lib\armv4" /LIBPATH:"%VC%\ce\lib\armv4" ^
  coredll.lib corelibc.lib /NODEFAULTLIB:oldnames.lib
if errorlevel 1 goto fail

echo === OK
exit /b 0
:fail
echo === BUILD FAILED
exit /b 1

@echo off
rem Builds HuCheck.exe (Windows CE, ARMv4): the unit's facts before VestaHome moves to the stub alone (hucheck.cpp).
rem needs.inc first: the vendor functions our code calls, from our sources (tools\make_hucheck_needs.py). Then the
rem PC test of pecheck.cpp. Runs on the unit only.
setlocal

if "%VS8%"=="" set VS8=C:\Program Files (x86)\Microsoft Visual Studio 8
set VC=%VS8%\VC
set SDK=%VS8%\SmartDevices\SDK\PocketPC2003
set PATH=%VS8%\Common7\IDE;%VC%\bin;%PATH%
set OUT=..\HuCheck

cd /d "%~dp0"
python ..\..\tools\make_hucheck_needs.py || goto fail
call .\test_pecheck.bat || goto fail
if not exist obj\arm mkdir obj\arm
if not exist "%OUT%" mkdir "%OUT%"

echo === ARM (Windows CE)
"%VC%\ce\bin\x86_arm\cl.exe" /nologo /O2 /W3 /GS- /TP /c hucheck.cpp pecheck.cpp /Foobj\arm\ ^
  /DUNICODE /D_UNICODE /DUNDER_CE=0x420 /D_WIN32_WCE=0x420 /DWINCE /DARM /D_ARM_ /DARMV4 /DNDEBUG ^
  /I"%SDK%\Include" /I"%VC%\ce\include"
if errorlevel 1 goto fail
"%VC%\ce\bin\x86_arm\link.exe" /nologo /SUBSYSTEM:WINDOWSCE,4.20 /MACHINE:ARM /OUT:"%OUT%\HuCheck.exe" ^
  obj\arm\hucheck.obj obj\arm\pecheck.obj /LIBPATH:"%SDK%\Lib\armv4" /LIBPATH:"%VC%\ce\lib\armv4" ^
  coredll.lib corelibc.lib /NODEFAULTLIB:oldnames.lib /NODEFAULTLIB:advapi32.lib
if errorlevel 1 goto fail

echo === OK
exit /b 0
:fail
echo === BUILD FAILED
exit /b 1

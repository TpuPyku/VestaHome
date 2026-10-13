@echo off
rem Builds VestaHome.exe (Windows CE, ARMv4) and VestaHomePC.exe (desktop x86, for testing)
rem with Visual Studio 2005 (Smart Device support + Pocket PC 2003 SDK).
setlocal

if "%VS8%"=="" set VS8=C:\Program Files (x86)\Microsoft Visual Studio 8
set VC=%VS8%\VC
set SDK=%VS8%\SmartDevices\SDK\PocketPC2003
set PATH=%VS8%\Common7\IDE;%VC%\bin;%PATH%
set SRC=home.cpp https.cpp
set OUT=..\VestaHome
set BRINC=/I"..\..\third_party\bearssl-0.6\inc" /I"..\..\third_party\compat"

cd /d "%~dp0"
if not exist obj\arm mkdir obj\arm
if not exist obj\pc  mkdir obj\pc
if not exist "%OUT%" mkdir "%OUT%"
if exist obj\bearssl_arm.lib if exist obj\bearssl_pc.lib goto bearssl_ok
call build_bearssl.bat
if errorlevel 1 goto fail
:bearssl_ok

echo === icon (app.rc, same .res for both builds)
rc /fo obj\app.res app.rc
if errorlevel 1 goto fail

echo === ARM (Windows CE)
"%VC%\ce\bin\x86_arm\cl.exe" /nologo /O2 /W3 /GS- /TP /c %SRC% /Foobj\arm\ ^
  /DUNICODE /D_UNICODE /DUNDER_CE=0x420 /D_WIN32_WCE=0x420 /DWINCE /DARM /D_ARM_ /DARMV4 /DNDEBUG ^
  /I"%SDK%\Include" /I"%VC%\ce\include" %BRINC%
if errorlevel 1 goto fail
"%VC%\ce\bin\x86_arm\link.exe" /nologo /SUBSYSTEM:WINDOWSCE,4.20 /MACHINE:ARM /OUT:"%OUT%\VestaHome.exe" ^
  obj\arm\*.obj obj\app.res obj\bearssl_arm.lib /LIBPATH:"%SDK%\Lib\armv4" /LIBPATH:"%VC%\ce\lib\armv4" ^
  coredll.lib corelibc.lib ws2.lib /NODEFAULTLIB:oldnames.lib /NODEFAULTLIB:advapi32.lib
if errorlevel 1 goto fail

echo === x86 (desktop test build)
"%VC%\bin\cl.exe" /nologo /O2 /W3 /MT /TP /c %SRC% /Foobj\pc\ ^
  /DUNICODE /D_UNICODE /DWIN32 /DNDEBUG /D_CRT_SECURE_NO_WARNINGS ^
  /I"%VC%\include" /I"%VC%\PlatformSDK\Include" %BRINC%
if errorlevel 1 goto fail
"%VC%\bin\link.exe" /nologo /SUBSYSTEM:WINDOWS /OUT:"%OUT%\VestaHomePC.exe" obj\pc\*.obj obj\app.res obj\bearssl_pc.lib ^
  /LIBPATH:"%VC%\lib" /LIBPATH:"%VC%\PlatformSDK\Lib" user32.lib gdi32.lib kernel32.lib shell32.lib ws2_32.lib winmm.lib
if errorlevel 1 goto fail

echo === OK
exit /b 0
:fail
echo === BUILD FAILED
exit /b 1

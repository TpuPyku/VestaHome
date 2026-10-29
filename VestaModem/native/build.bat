@echo off
rem Builds VestaModem.dll (Windows CE, ARMv4: the USB modem's client driver, loaded by USBD in device.exe, and its
rem COM port) and runs its desktop test (test_mdm.cpp: the parts with no USB) with Visual Studio 2005. Build it
rem before the installer (it packs the dll).
setlocal

if "%VS8%"=="" set VS8=C:\Program Files (x86)\Microsoft Visual Studio 8
set VC=%VS8%\VC
set SDK=%VS8%\SmartDevices\SDK\PocketPC2003
set PATH=%VS8%\Common7\IDE;%VC%\bin;%PATH%
set OUT=..\VestaModem

cd /d "%~dp0"
if not exist obj\arm mkdir obj\arm
if not exist obj\pc  mkdir obj\pc
if not exist "%OUT%" mkdir "%OUT%"

echo === ARM (Windows CE)
"%VC%\ce\bin\x86_arm\cl.exe" /nologo /O2 /W3 /GS- /TP /c vmodem.cpp mdm.cpp /Foobj\arm\ ^
  /DUNICODE /D_UNICODE /DUNDER_CE=0x420 /D_WIN32_WCE=0x420 /DWINCE /DARM /D_ARM_ /DARMV4 /DNDEBUG ^
  /I"%SDK%\Include" /I"%VC%\ce\include"
if errorlevel 1 goto fail
"%VC%\ce\bin\x86_arm\link.exe" /nologo /DLL /SUBSYSTEM:WINDOWSCE,4.20 /MACHINE:ARM ^
  /OUT:"%OUT%\VestaModem.dll" /IMPLIB:obj\arm\VestaModem.lib /DEF:vmodem.def obj\arm\vmodem.obj obj\arm\mdm.obj ^
  /LIBPATH:"%SDK%\Lib\armv4" /LIBPATH:"%VC%\ce\lib\armv4" ^
  coredll.lib corelibc.lib /NODEFAULTLIB:oldnames.lib
if errorlevel 1 goto fail

echo === x86 test
rem COVER=1: the tests built for coverage (/Od /Zi, OpenCppCoverage)
set OPT=/O2
set DBG=
if "%COVER%"=="1" set OPT=/Od /Zi
if "%COVER%"=="1" set DBG=/DEBUG
"%VC%\bin\cl.exe" /nologo %OPT% /W3 /MT /TP test_mdm.cpp mdm.cpp /Foobj\pc\ /Fdobj\pc\ /Feobj\pc\test_mdm.exe ^
  /DUNICODE /D_UNICODE /DWIN32 /DNDEBUG /D_CRT_SECURE_NO_WARNINGS ^
  /I"%VC%\include" /I"%VC%\PlatformSDK\Include" ^
  /link %DBG% /LIBPATH:"%VC%\lib" /LIBPATH:"%VC%\PlatformSDK\Lib" kernel32.lib
if errorlevel 1 goto fail
obj\pc\test_mdm.exe
if errorlevel 1 goto fail
rem the driver over a made-up USBD (the SDK's usbdi.h after the desktop headers)
"%VC%\bin\cl.exe" /nologo %OPT% /W3 /MT /TP /EHa test_vmodem.cpp mdm.cpp /Foobj\pc\ /Fdobj\pc\ /Feobj\pc\test_vmodem.exe ^
  /DUNICODE /D_UNICODE /DWIN32 /DNDEBUG /D_CRT_SECURE_NO_WARNINGS ^
  /I"%VC%\include" /I"%VC%\PlatformSDK\Include" /I"%SDK%\Include" ^
  /link %DBG% /LIBPATH:"%VC%\lib" /LIBPATH:"%VC%\PlatformSDK\Lib" kernel32.lib advapi32.lib
if errorlevel 1 goto fail
obj\pc\test_vmodem.exe
if errorlevel 1 goto fail

echo === OK
exit /b 0
:fail
echo === BUILD FAILED
exit /b 1

@echo off
rem Builds VestaModem.dll (Windows CE, ARMv4: the USB modem's client driver, loaded by USBD in device.exe) with
rem Visual Studio 2005. Build it before the installer (it packs the dll).
setlocal

if "%VS8%"=="" set VS8=C:\Program Files (x86)\Microsoft Visual Studio 8
set VC=%VS8%\VC
set SDK=%VS8%\SmartDevices\SDK\PocketPC2003
set PATH=%VS8%\Common7\IDE;%VC%\bin;%PATH%
set OUT=..\VestaModem

cd /d "%~dp0"
if not exist obj\arm mkdir obj\arm
if not exist "%OUT%" mkdir "%OUT%"

echo === ARM (Windows CE)
"%VC%\ce\bin\x86_arm\cl.exe" /nologo /O2 /W3 /GS- /TP /c vmodem.cpp /Foobj\arm\ ^
  /DUNICODE /D_UNICODE /DUNDER_CE=0x420 /D_WIN32_WCE=0x420 /DWINCE /DARM /D_ARM_ /DARMV4 /DNDEBUG ^
  /I"%SDK%\Include" /I"%VC%\ce\include"
if errorlevel 1 goto fail
"%VC%\ce\bin\x86_arm\link.exe" /nologo /DLL /SUBSYSTEM:WINDOWSCE,4.20 /MACHINE:ARM ^
  /OUT:"%OUT%\VestaModem.dll" /IMPLIB:obj\arm\VestaModem.lib /DEF:vmodem.def obj\arm\vmodem.obj ^
  /LIBPATH:"%SDK%\Lib\armv4" /LIBPATH:"%VC%\ce\lib\armv4" ^
  coredll.lib corelibc.lib /NODEFAULTLIB:oldnames.lib
if errorlevel 1 goto fail

echo === OK
exit /b 0
:fail
echo === BUILD FAILED
exit /b 1

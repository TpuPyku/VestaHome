@echo off
rem Builds VestaTether.dll and TetherProbe.exe (Windows CE, ARMv4: the phone's USB tethering, the probe) and runs their
rem desktop tests (test_rndis: the messages; test_vtether: the driver over a made-up USBD and phone).
setlocal
if "%VS8%"=="" set VS8=C:\Program Files (x86)\Microsoft Visual Studio 8
set VC=%VS8%\VC
set SDK=%VS8%\SmartDevices\SDK\PocketPC2003
set PATH=%VS8%\Common7\IDE;%VC%\bin;%PATH%
set OUT=..\VestaTether
rem BearSSL (the ADB key, 2026-11-12): VestaHome's libraries of the same copy
set VH=..\..\VestaHome\native
set BR=..\..\third_party\bearssl-0.6
set COMPAT=..\..\third_party\compat
cd /d "%~dp0"
if not exist obj\arm mkdir obj\arm
if not exist obj\pc  mkdir obj\pc
if not exist "%OUT%" mkdir "%OUT%"
if not exist %VH%\obj\bearssl_arm.lib call %VH%\build_bearssl.bat || goto fail

echo === ARM (Windows CE)
"%VC%\ce\bin\x86_arm\cl.exe" /nologo /O2 /W3 /GS- /TP /c vtether.cpp rndis.cpp adb.cpp probe.cpp /Foobj\arm\ ^
  /DUNICODE /D_UNICODE /DUNDER_CE=0x420 /D_WIN32_WCE=0x420 /DWINCE /DARM /D_ARM_ /DARMV4 /DNDEBUG ^
  /I"%BR%\inc" /I"%COMPAT%" /I"%SDK%\Include" /I"%VC%\ce\include"
if errorlevel 1 goto fail
rem the network adapter (NDIS 5.1 miniport, step 2): the unit's NDIS only
"%VC%\ce\bin\x86_arm\cl.exe" /nologo /O2 /W3 /GS- /TP /c ndismp.cpp /Foobj\arm\ ^
  /DUNICODE /D_UNICODE /DUNDER_CE=0x420 /D_WIN32_WCE=0x420 /DWINCE /DARM /D_ARM_ /DARMV4 /DNDEBUG ^
  /DNDIS_MINIPORT_DRIVER /DNDIS51_MINIPORT /DBINARY_COMPATIBLE=1 /Isdkfix /I"%SDK%\Include" /I"%VC%\ce\include"
if errorlevel 1 goto fail
"%VC%\ce\bin\x86_arm\link.exe" /nologo /DLL /SUBSYSTEM:WINDOWSCE,4.20 /MACHINE:ARM ^
  /OUT:"%OUT%\VestaTether.dll" /IMPLIB:obj\arm\VestaTether.lib /DEF:vtether.def obj\arm\vtether.obj obj\arm\rndis.obj ^
  obj\arm\adb.obj obj\arm\ndismp.obj %VH%\obj\bearssl_arm.lib ^
  /LIBPATH:"%SDK%\Lib\armv4" /LIBPATH:"%VC%\ce\lib\armv4" coredll.lib corelibc.lib ndis.lib /NODEFAULTLIB:oldnames.lib
if errorlevel 1 goto fail
"%VC%\ce\bin\x86_arm\link.exe" /nologo /SUBSYSTEM:WINDOWSCE,4.20 /MACHINE:ARM /OUT:"%OUT%\TetherProbe.exe" obj\arm\probe.obj ^
  obj\arm\adb.obj obj\arm\rndis.obj %VH%\obj\bearssl_arm.lib ^
  /LIBPATH:"%SDK%\Lib\armv4" /LIBPATH:"%VC%\ce\lib\armv4" coredll.lib corelibc.lib /NODEFAULTLIB:oldnames.lib
if errorlevel 1 goto fail

echo === x86 tests
call .\test_rndis.bat || goto fail
call .\test_vtether.bat || goto fail
call .\test_adb.bat || goto fail
echo === OK
exit /b 0
:fail
echo === FAILED
exit /b 1

@echo off
rem Builds VestaHome.exe (Windows CE, ARMv4) and VestaHomePC.exe (desktop x86, for testing)
rem with Visual Studio 2005 (Smart Device support + Pocket PC 2003 SDK).
setlocal

if "%VS8%"=="" set VS8=C:\Program Files (x86)\Microsoft Visual Studio 8
set VC=%VS8%\VC
set SDK=%VS8%\SmartDevices\SDK\PocketPC2003
set PATH=%VS8%\Common7\IDE;%VC%\bin;%PATH%
set SRC=home.cpp musdir.cpp netcat.cpp netcache.cpp country.cpp video.cpp wifi.cpp wifiie.cpp ramlog.cpp inikey.cpp obd.cpp btrf.cpp dialloc.cpp drawwhy.cpp btowner.cpp trackend.cpp service.cpp numpad.cpp pbcache.cpp netmirror.cpp presetpos.cpp zonepart.cpp photo.cpp cover.cpp radiodb.cpp ..\..\VHCheck\native\pecheck.cpp ..\..\VHCheck\native\pefile.cpp wglayout.cpp callwin.cpp webcover.cpp
set OUT=..\VestaHome
set BRINC=/I"..\..\third_party\bearssl-0.6\inc" /I"..\..\third_party\compat" /I"..\..\third_party\tjpgd"
rem TJpgDec (photos): C, compiled on its own (/TC)
set JPG=..\..\third_party\tjpgd\tjpgd.c
rem VestaCore (shared with OSMapsCE): a working copy of the VestaCore repo next to this repo if there is one
rem (development: its latest code), otherwise the submodule core\ (a clone from GitHub)
set CORE=%~dp0..\..\..\VestaCore
if not exist "%CORE%\vgps.cpp" set CORE=%~dp0..\..\core
if not exist "%CORE%\vgps.cpp" goto nocore
echo === VestaCore: %CORE%
set SRC=%SRC% "%CORE%\vgps.cpp" "%CORE%\vlogdir.cpp" "%CORE%\vtask.cpp" "%CORE%\vnight.cpp" "%CORE%\vkeys.cpp" "%CORE%\vfavs.cpp" "%CORE%\vhttp.cpp" "%CORE%\vhap.cpp" "%CORE%\vnav.cpp" "%CORE%\vfont.cpp" "%CORE%\vjson.cpp" "%CORE%\vlicense.cpp" "%CORE%\vsecret.cpp" "%CORE%\vrvc.cpp" "%CORE%\vtouch.cpp" "%CORE%\vupdate.cpp"
set BRINC=%BRINC% /I"%CORE%"
rem the modem's operators' presets (Settings -> Network): VestaModem's mdm.cpp (its tests: VestaModem\native)
set SRC=%SRC% "%~dp0..\..\VestaModem\native\mdm.cpp"
set BRINC=%BRINC% /I"%~dp0..\..\VestaModem\native"

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
"%VC%\ce\bin\x86_arm\cl.exe" /nologo /O2 /W3 /GS- /TC /c %JPG% /Foobj\arm\ ^
  /DUNDER_CE=0x420 /D_WIN32_WCE=0x420 /DWINCE /DARM /D_ARM_ /DARMV4 /DNDEBUG /I"%SDK%\Include" /I"%VC%\ce\include"
if errorlevel 1 goto fail
"%VC%\ce\bin\x86_arm\link.exe" /nologo /SUBSYSTEM:WINDOWSCE,4.20 /MACHINE:ARM /OUT:"%OUT%\VestaHome.exe" ^
  obj\arm\*.obj obj\app.res obj\bearssl_arm.lib /LIBPATH:"%SDK%\Lib\armv4" /LIBPATH:"%VC%\ce\lib\armv4" ^
  coredll.lib corelibc.lib ws2.lib /NODEFAULTLIB:oldnames.lib /NODEFAULTLIB:advapi32.lib
if errorlevel 1 goto fail

echo === x86 (desktop test build)
rem (the PC only: GpsSim's lines for the demo head unit's drive, vh_demo.inc)
"%VC%\bin\cl.exe" /nologo /O2 /W3 /MT /TP /c %SRC% ..\..\GpsSim\native\simnmea.cpp /Foobj\pc\ ^
  /DUNICODE /D_UNICODE /DWIN32 /DNDEBUG /D_CRT_SECURE_NO_WARNINGS ^
  /I"%VC%\include" /I"%VC%\PlatformSDK\Include" %BRINC%
if errorlevel 1 goto fail
"%VC%\bin\cl.exe" /nologo /O2 /W3 /MT /TC /c %JPG% /Foobj\pc\ /DWIN32 /DNDEBUG /D_CRT_SECURE_NO_WARNINGS ^
  /I"%VC%\include" /I"%VC%\PlatformSDK\Include"
if errorlevel 1 goto fail
"%VC%\bin\link.exe" /nologo /SUBSYSTEM:WINDOWS /OUT:"%OUT%\VestaHomePC.exe" obj\pc\*.obj obj\app.res obj\bearssl_pc.lib ^
  /LIBPATH:"%VC%\lib" /LIBPATH:"%VC%\PlatformSDK\Lib" user32.lib gdi32.lib kernel32.lib shell32.lib ws2_32.lib winmm.lib
if errorlevel 1 goto fail

echo === OK
exit /b 0
:nocore
echo === VestaCore not found: git submodule update --init (core\) or the sibling repo ..\..\..\VestaCore
exit /b 1
:fail
echo === BUILD FAILED
exit /b 1

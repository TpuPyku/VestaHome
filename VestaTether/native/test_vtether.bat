@echo off
rem Builds and runs test_vtether.exe (the probe over a made-up USBD; VS2005 x86) in obj\pc\. Exit code: the failed checks.
rem "build": only the program (the mutator runs it). COVER=1: for coverage.
setlocal
if "%VS8%"=="" set VS8=C:\Program Files (x86)\Microsoft Visual Studio 8
set VC=%VS8%\VC
set SDK=%VS8%\SmartDevices\SDK\PocketPC2003
set PATH=%VS8%\Common7\IDE;%VC%\bin;%PATH%
cd /d "%~dp0"
if not exist obj\pc mkdir obj\pc
set OPT=/O2
if "%COVER%"=="1" set OPT=/Od /Zi
set VH=..\..\VestaHome\native
set BR=..\..\third_party\bearssl-0.6
set COMPAT=..\..\third_party\compat
if not exist %VH%\obj\bearssl_pc.lib call %VH%\build_bearssl.bat || exit /b 1
"%VC%\bin\cl.exe" /nologo %OPT% /W3 /MT /TP /EHa test_vtether.cpp rndis.cpp adb.cpp /Foobj\pc\ /Fdobj\pc\ /Feobj\pc\test_vtether.exe ^
  /DUNICODE /D_UNICODE /DWIN32 /DNDEBUG /D_CRT_SECURE_NO_WARNINGS ^
  /I"%BR%\inc" /I"%COMPAT%" /I"%COMPAT%\pc" /I"%VC%\include" /I"%VC%\PlatformSDK\Include" /I"%SDK%\Include" ^
  /link /DEBUG /LIBPATH:"%VC%\lib" /LIBPATH:"%VC%\PlatformSDK\Lib" kernel32.lib advapi32.lib %VH%\obj\bearssl_pc.lib >obj\pc\build_vt.log || (type obj\pc\build_vt.log & exit /b 1)
if "%1"=="build" exit /b 0
obj\pc\test_vtether.exe

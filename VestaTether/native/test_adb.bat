@echo off
rem Builds and runs test_adb.exe (the ADB messages, key and session; VS2005 x86) in obj\pc\. Exit code: the failed checks.
rem "build": only the program (the mutator runs it). COVER=1: for coverage. BearSSL: VestaHome's obj\bearssl_pc.lib.
setlocal
if "%VS8%"=="" set VS8=C:\Program Files (x86)\Microsoft Visual Studio 8
set VC=%VS8%\VC
set PATH=%VS8%\Common7\IDE;%VC%\bin;%PATH%
set VH=..\..\VestaHome\native
set BR=..\..\third_party\bearssl-0.6
set COMPAT=..\..\third_party\compat
cd /d "%~dp0"
if not exist obj\pc mkdir obj\pc
if not exist %VH%\obj\bearssl_pc.lib call %VH%\build_bearssl.bat || exit /b 1
set OPT=/O2
if "%COVER%"=="1" set OPT=/Od /Zi
"%VC%\bin\cl.exe" /nologo %OPT% /W3 /MT /TP test_adb.cpp adb.cpp rndis.cpp /Foobj\pc\ /Fdobj\pc\ /Feobj\pc\test_adb.exe ^
  /D_CRT_SECURE_NO_WARNINGS /I"%BR%\inc" /I"%COMPAT%" /I"%COMPAT%\pc" /I"%VC%\include" /I"%VC%\PlatformSDK\Include" ^
  /link /DEBUG /LIBPATH:"%VC%\lib" /LIBPATH:"%VC%\PlatformSDK\Lib" %VH%\obj\bearssl_pc.lib >obj\pc\build_adb.log || (type obj\pc\build_adb.log & exit /b 1)
if "%1"=="build" exit /b 0
obj\pc\test_adb.exe

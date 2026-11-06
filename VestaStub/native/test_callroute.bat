@echo off
rem Builds and runs test_callroute.exe (VS2005 x86) in obj\pc\. Exit code: the failed checks. "build": only the program.
setlocal
if "%VS8%"=="" set VS8=C:\Program Files (x86)\Microsoft Visual Studio 8
set VC=%VS8%\VC
set PATH=%VS8%\Common7\IDE;%VC%\bin;%PATH%
cd /d "%~dp0"
if not exist obj\pc mkdir obj\pc
"%VC%\bin\cl.exe" /nologo /W3 /MT /TP test_callroute.cpp /Foobj\pc\ /Fdobj\pc\ /Feobj\pc\test_callroute.exe ^
  /I"%VC%\include" /I"%VC%\PlatformSDK\Include" /link /LIBPATH:"%VC%\lib" /LIBPATH:"%VC%\PlatformSDK\Lib" >obj\pc\build_test.log || (type obj\pc\build_test.log & exit /b 1)
if "%1"=="build" exit /b 0
obj\pc\test_callroute.exe

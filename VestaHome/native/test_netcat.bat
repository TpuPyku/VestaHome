@echo off
rem Builds and runs test_netcat.exe (netcat.cpp + VestaCore vjson.cpp on the PC, VS2005 x86) in obj\; exit code:
rem the failed checks. "build": only builds (the mutator runs it). COVER=1: /Od /Zi (OpenCppCoverage, then crapper).
setlocal
set OPT=/O2
if "%COVER%"=="1" set OPT=/Od /Zi
if "%VS8%"=="" set VS8=C:\Program Files (x86)\Microsoft Visual Studio 8
set VC=%VS8%\VC
set PATH=%VS8%\Common7\IDE;%VC%\bin;%PATH%
cd /d "%~dp0"
set CORE=%~dp0..\..\..\VestaCore
if not exist "%CORE%\vjson.cpp" set CORE=%~dp0..\..\core
if not exist obj mkdir obj
"%VC%\bin\cl.exe" /nologo %OPT% /W3 /MT /TP test_netcat.cpp netcat.cpp "%CORE%\vjson.cpp" /Foobj\ /Fdobj\ /Feobj\test_netcat.exe ^
  /DUNICODE /D_UNICODE /DWIN32 /D_CRT_SECURE_NO_WARNINGS /I"%CORE%" /I"%VC%\include" /I"%VC%\PlatformSDK\Include" ^
  /link /LIBPATH:"%VC%\lib" /LIBPATH:"%VC%\PlatformSDK\Lib" kernel32.lib >obj\build_netcat.log || (type obj\build_netcat.log & exit /b 1)
if "%1"=="build" exit /b 0
obj\test_netcat.exe

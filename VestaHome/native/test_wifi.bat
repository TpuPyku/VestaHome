@echo off
rem Builds and runs test_wifi.exe (wifi.cpp's threads on the PC, ~35 s, VS2005 x86) in obj\; exit code: the failed checks.
rem "build": only builds (the mutator runs it). COVER=1: /Od /Zi (OpenCppCoverage, then crapper).
setlocal
set OPT=/O2
if "%COVER%"=="1" set OPT=/Od /Zi
if "%VS8%"=="" set VS8=C:\Program Files (x86)\Microsoft Visual Studio 8
set VC=%VS8%\VC
set PATH=%VS8%\Common7\IDE;%VC%\bin;%PATH%
cd /d "%~dp0"
if not exist obj mkdir obj
"%VC%\bin\cl.exe" /nologo %OPT% /W3 /MT /TP test_wifi.cpp wifi.cpp wifiie.cpp /Foobj\ /Fdobj\ /Feobj\test_wifi.exe ^
  /DUNICODE /D_UNICODE /DWIN32 /D_CRT_SECURE_NO_WARNINGS /I"%VC%\include" /I"%VC%\PlatformSDK\Include" ^
  /link /LIBPATH:"%VC%\lib" /LIBPATH:"%VC%\PlatformSDK\Lib" kernel32.lib user32.lib >obj\build_wifi.log || (type obj\build_wifi.log & exit /b 1)
if "%1"=="build" exit /b 0
obj\test_wifi.exe

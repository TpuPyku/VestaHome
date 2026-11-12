@echo off
rem Builds and runs test_netmirror.exe (netmirror.cpp on the PC, VS2005 x86) in obj\; exit
rem code: the failed checks. "build": only builds (the mutator runs it). COVER=1: /Od /Zi.
setlocal
set OPT=/O2
if "%COVER%"=="1" set OPT=/Od /Zi
if "%VS8%"=="" set VS8=C:\Program Files (x86)\Microsoft Visual Studio 8
set VC=%VS8%\VC
set PATH=%VS8%\Common7\IDE;%VC%\bin;%PATH%
cd /d "%~dp0"
if not exist obj mkdir obj
"%VC%\bin\cl.exe" /nologo %OPT% /W3 /MT /TP test_netmirror.cpp netmirror.cpp /Foobj\ /Fdobj\ /Feobj\test_netmirror.exe ^
  /DUNICODE /D_UNICODE /DWIN32 /D_CRT_SECURE_NO_WARNINGS /I"%VC%\include" /I"%VC%\PlatformSDK\Include" ^
  /link /LIBPATH:"%VC%\lib" /LIBPATH:"%VC%\PlatformSDK\Lib" kernel32.lib >obj\build_netmirror.log || (type obj\build_netmirror.log & exit /b 1)
if "%1"=="build" exit /b 0
obj\test_netmirror.exe

@echo off
rem Builds and runs test_simnmea.exe (simnmea.cpp on the PC, VS2005 x86) in obj\; exit code: the failed checks.
setlocal
if "%VS8%"=="" set VS8=C:\Program Files (x86)\Microsoft Visual Studio 8
set VC=%VS8%\VC
set PATH=%VS8%\Common7\IDE;%VC%\bin;%PATH%
set OPT=/Od /Zi
cd /d "%~dp0"
if not exist obj mkdir obj
"%VC%\bin\cl.exe" /nologo %OPT% /W3 /MT /TP test_simnmea.cpp simnmea.cpp /Foobj\ /Fdobj\ /Feobj\test_simnmea.exe ^
  /DWIN32 /D_CRT_SECURE_NO_WARNINGS /I"%VC%\include" /I"%VC%\PlatformSDK\Include" ^
  /link /LIBPATH:"%VC%\lib" /LIBPATH:"%VC%\PlatformSDK\Lib" kernel32.lib >obj\build_test.log || (type obj\build_test.log & exit /b 1)
if "%1"=="build" exit /b 0
obj\test_simnmea.exe

@echo off
rem Builds and runs test_regwatch.exe (VS2005 x86) in obj\pc\. Exit code: the failed checks.
setlocal
if "%VS8%"=="" set VS8=C:\Program Files (x86)\Microsoft Visual Studio 8
set VC=%VS8%\VC
set PATH=%VS8%\Common7\IDE;%VC%\bin;%PATH%
cd /d "%~dp0"
if not exist obj\pc mkdir obj\pc
"%VC%\bin\cl.exe" /nologo /W3 /MT /TP /D_CRT_SECURE_NO_WARNINGS test_regwatch.cpp /Foobj\pc\ /Fdobj\pc\ /Feobj\pc\test_regwatch.exe ^
  /I"%VC%\include" /I"%VC%\PlatformSDK\Include" /link /LIBPATH:"%VC%\lib" /LIBPATH:"%VC%\PlatformSDK\Lib" >obj\pc\build_rw.log || (type obj\pc\build_rw.log & exit /b 1)
obj\pc\test_regwatch.exe

@echo off
rem Builds and runs test_pbcache.exe (pbcache.cpp + VestaCore vsecret.cpp with BearSSL on the PC, VS2005 x86) in obj\; exit
rem code: the failed checks. "build": only builds (the mutator runs it). COVER=1: /Od /Zi.
setlocal
set OPT=/O2
if "%COVER%"=="1" set OPT=/Od /Zi
if "%VS8%"=="" set VS8=C:\Program Files (x86)\Microsoft Visual Studio 8
set VC=%VS8%\VC
set PATH=%VS8%\Common7\IDE;%VC%\bin;%PATH%
cd /d "%~dp0"
if not exist obj mkdir obj
if not exist obj\bearssl_pc.lib call build_bearssl.bat
set CORE=%~dp0..\..\..\VestaCore
if not exist "%CORE%\vlicense.cpp" set CORE=%~dp0..\..\core
"%VC%\bin\cl.exe" /nologo %OPT% /W3 /MT /TP test_pbcache.cpp pbcache.cpp "%CORE%\vsecret.cpp" "%CORE%\vlicense.cpp" /Foobj\ /Fdobj\ /Feobj\test_pbcache.exe ^
  /DUNICODE /D_UNICODE /DWIN32 /D_CRT_SECURE_NO_WARNINGS /I"%VC%\include" /I"%VC%\PlatformSDK\Include" ^
  /I"..\..\third_party\bearssl-0.6\inc" /I"..\..\third_party\compat" /I"%CORE%" ^
  /link /LIBPATH:"%VC%\lib" /LIBPATH:"%VC%\PlatformSDK\Lib" obj\bearssl_pc.lib kernel32.lib >obj\build_pbcache.log || (type obj\build_pbcache.log & exit /b 1)
if "%1"=="build" exit /b 0
obj\test_pbcache.exe

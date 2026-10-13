@echo off
rem Compiles the PC-side drill-package guard test with the x86 compiler of the same VS2005
rem that builds the ARM installer (VC\bin\cl.exe is the native x86 host compiler, the CE
rem ARM one lives in VC\ce\bin\x86_arm). It includes install/drilltag.h unchanged, so the
rem test runs the very code the installer uses. UNICODE is defined so CreateFile maps to
rem the wide variant, exactly as in install/build.bat.
rem   tools\drilltag_hosttest.bat
setlocal
if "%VS8%"=="" set VS8=C:\Program Files (x86)\Microsoft Visual Studio 8
set OUT=..\obj\host

cd /d "%~dp0"
if not exist "%OUT%" mkdir "%OUT%"

call "%VS8%\VC\bin\vcvars32.bat" >nul
cl.exe /nologo /O2 /W3 /DUNICODE /D_UNICODE /D_CRT_SECURE_NO_WARNINGS /Fe..\obj\host\drilltag_hosttest.exe /Fo..\obj\host\ drilltag_hosttest.c
if errorlevel 1 goto fail

..\obj\host\drilltag_hosttest.exe
set RC=%ERRORLEVEL%
exit /b %RC%

:fail
echo === BUILD FAILED
exit /b 1

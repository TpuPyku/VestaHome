@echo off
rem Compiles the PC-side MD5 self-test with the x86 compiler of the same VS2005 that
rem builds the ARM installer (VC\bin\cl.exe is the native x86 host compiler, the CE
rem ARM one lives in VC\ce\bin\x86_arm). No arguments: RFC 1321 vectors.
rem   tools\md5_hosttest.bat            -> vectors
rem   tools\md5_hosttest.bat <file>...  -> "md5  path" lines
setlocal
if "%VS8%"=="" set VS8=C:\Program Files (x86)\Microsoft Visual Studio 8
set OUT=..\obj\host

cd /d "%~dp0"
if not exist "%OUT%" mkdir "%OUT%"

call "%VS8%\VC\bin\vcvars32.bat" >nul
cl.exe /nologo /O2 /W3 /D_CRT_SECURE_NO_WARNINGS /Fe..\obj\host\md5_hosttest.exe /Fo..\obj\host\ md5_hosttest.c
if errorlevel 1 goto fail

..\obj\host\md5_hosttest.exe %*
set RC=%ERRORLEVEL%
exit /b %RC%

:fail
echo === BUILD FAILED
exit /b 1

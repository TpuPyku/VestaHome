@echo off
rem Builds SPIPowerInstall.exe (Windows CE, ARMv4) - safe swap/reboot/rollback tool.
setlocal
if "%VS8%"=="" set VS8=C:\Program Files (x86)\Microsoft Visual Studio 8
set VC=%VS8%\VC
set SDK=%VS8%\SmartDevices\SDK\PocketPC2003
set PATH=%VS8%\Common7\IDE;%VC%\bin;%VC%\ce\bin\x86_arm;%PATH%
set OUT=..\out

cd /d "%~dp0"
if not exist obj\arm mkdir obj\arm
if not exist "%OUT%" mkdir "%OUT%"

"%VC%\ce\bin\x86_arm\cl.exe" /nologo /O2 /W3 /GS- /TP /c spipower_install.cpp /Foobj\arm\ ^
  /DUNICODE /D_UNICODE /DUNDER_CE=0x420 /D_WIN32_WCE=0x420 /DWINCE /DARM /D_ARM_ /DARMV4 /DNDEBUG ^
  /I"%SDK%\Include" /I"%VC%\ce\include"
if errorlevel 1 goto fail

"%VC%\ce\bin\x86_arm\link.exe" /nologo /SUBSYSTEM:WINDOWSCE,4.20 /MACHINE:ARM ^
  /OUT:"%OUT%\SPIPowerInstall.exe" obj\arm\spipower_install.obj ^
  /LIBPATH:"%SDK%\Lib\armv4" /LIBPATH:"%VC%\ce\lib\armv4" ^
  coredll.lib corelibc.lib /NODEFAULTLIB:oldnames.lib
if errorlevel 1 goto fail

echo === OK
exit /b 0
:fail
echo === BUILD FAILED
exit /b 1

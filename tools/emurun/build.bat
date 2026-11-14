@echo off
rem Builds emurun.exe (Windows CE, ARMv4) for the Device Emulator (see emurun.cpp) into obj\. Copy it to the emulator's
rem shared folder as StaticStore\2577\autorun.exe and StaticStore\emurun\emurun.exe.
setlocal
if "%VS8%"=="" set VS8=C:\Program Files (x86)\Microsoft Visual Studio 8
set VC=%VS8%\VC
set SDK=%VS8%\SmartDevices\SDK\PocketPC2003
set PATH=%VS8%\Common7\IDE;%VC%\bin;%PATH%
cd /d "%~dp0"
if not exist obj mkdir obj
"%VC%\ce\bin\x86_arm\cl.exe" /nologo /O2 /W3 /GS- /TP /c emurun.cpp /Foobj\ ^
  /DUNICODE /D_UNICODE /DUNDER_CE=0x420 /D_WIN32_WCE=0x420 /DWINCE /DARM /D_ARM_ /DARMV4 /DNDEBUG ^
  /I"%SDK%\Include" /I"%VC%\ce\include"
if errorlevel 1 exit /b 1
"%VC%\ce\bin\x86_arm\link.exe" /nologo /SUBSYSTEM:WINDOWSCE,4.20 /MACHINE:ARM /OUT:obj\emurun.exe obj\emurun.obj ^
  /LIBPATH:"%SDK%\Lib\armv4" /LIBPATH:"%VC%\ce\lib\armv4" coredll.lib corelibc.lib /NODEFAULTLIB:oldnames.lib
if errorlevel 1 exit /b 1
echo === OK

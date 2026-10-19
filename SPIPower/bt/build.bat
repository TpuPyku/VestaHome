@echo off
rem Builds FLC_BLUETOOTH_SDK.dll (Windows CE, ARMv4) -- the proxy of the stock shell's Bluetooth SDK; it forwards
rem every export to the original, renamed FLC_BLUETOOTH_SDK_orig.dll, and publishes the events (btshm.h).
setlocal
if "%VS8%"=="" set VS8=C:\Program Files (x86)\Microsoft Visual Studio 8
set VC=%VS8%\VC
set SDK=%VS8%\SmartDevices\SDK\PocketPC2003
set PATH=%VS8%\Common7\IDE;%VC%\bin;%VC%\ce\bin\x86_arm;%PATH%
set OUT=..\out

cd /d "%~dp0"
if not exist obj\arm mkdir obj\arm
if not exist "%OUT%" mkdir "%OUT%"

rem thunks.s + flc.def + g_btproxy.h from the real DLL's exports
python gen_thunks_bt.py "..\..\..\MAIN\ResidentFlash\navigation\app\FLC_BLUETOOTH_SDK.dll"
if errorlevel 1 goto fail

"%VC%\ce\bin\x86_arm\armasm.exe" thunks.s -o obj\arm\thunks.obj
if errorlevel 1 goto fail

"%VC%\ce\bin\x86_arm\cl.exe" /nologo /O2 /W3 /GS- /TP /c bt_proxy.cpp /Foobj\arm\ ^
  /DUNICODE /D_UNICODE /DUNDER_CE=0x420 /D_WIN32_WCE=0x420 /DWINCE /DARM /D_ARM_ /DARMV4 /DNDEBUG ^
  /I"%SDK%\Include" /I"%VC%\ce\include"
if errorlevel 1 goto fail

"%VC%\ce\bin\x86_arm\link.exe" /nologo /DLL /SUBSYSTEM:WINDOWSCE,4.20 /MACHINE:ARM ^
  /OUT:"%OUT%\FLC_BLUETOOTH_SDK.dll" /DEF:flc.def obj\arm\*.obj ^
  /LIBPATH:"%SDK%\Lib\armv4" /LIBPATH:"%VC%\ce\lib\armv4" ^
  coredll.lib corelibc.lib /NODEFAULTLIB:oldnames.lib
if errorlevel 1 goto fail

echo === OK
exit /b 0
:fail
echo === BUILD FAILED
exit /b 1

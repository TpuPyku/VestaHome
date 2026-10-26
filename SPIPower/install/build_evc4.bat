@echo off
rem Builds VestaHomeInstall.exe (Windows CE, ARMv4) without Visual Studio 2005 - as VestaStub\native\build_evc4.bat:
rem the ARM compile and link with eVC4's toolchain (clarm 12.20 + link 6.24) and the Pocket PC 2003 SDK, the
rem resources (the payload, the proxies, the stub) with rc.exe of the Visual Studio 2019 Build Tools. build.bat
rem (VS2005) stays the reference build. Toolchains, overridable from the environment:
rem   EVC4   - the folder with bin\clarm.exe, bin\link.exe (bin\cvtres.exe there too: link calls it for the .res);
rem   SDK    - the Pocket PC 2003 SDK;
rem   VS2019 - the Build Tools (only its rc.exe is used).
rem The packed parts come from the repo: VestaHome\VestaHome\VestaHome.exe, VestaNMEA, VestaModem, VestaStub (their
rem build output is committed), the proxies SPIPower\out\SPIPowerMgr.dll and FLC_BLUETOOTH_SDK.dll (committed too:
rem fingerprints.h holds their MD5). eVC4 reads the source in the ANSI code page only, so the UTF-8 source is
rem converted to CP1251 into obj\evc4_arm\ first.
setlocal

if "%EVC4%"=="" set EVC4=C:\eVC4
if "%SDK%"=="" set SDK=C:\PPC2003SDK\POCKET PC 2003
if "%VS2019%"=="" set VS2019=C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools
set OUT=..\out

cd /d "%~dp0"
if not exist "%EVC4%\bin\clarm.exe" goto no_evc
if not exist "%SDK%\Lib\Armv4\coredll.lib" goto no_evc
if not exist "%VS2019%\VC\Auxiliary\Build\vcvars32.bat" goto no_rc
if not exist "%OUT%\SPIPowerMgr.dll" goto no_proxy
if not exist "%OUT%\FLC_BLUETOOTH_SDK.dll" goto no_proxy
if not exist obj\evc4_arm mkdir obj\evc4_arm

rem VestaHome (exe, Skin, radio.txt, bass) -> vh_payload.bin -> a resource
python make_vh_payload.py || goto fail
cmd /c ""%VS2019%\VC\Auxiliary\Build\vcvars32.bat" >nul && rc.exe /nologo /fo obj\evc4_arm\vh_payload.res resources.rc" || goto fail

powershell -NoProfile -Command "$t = [IO.File]::ReadAllText('%~dp0spipower_install.cpp', [Text.Encoding]::UTF8); [IO.File]::WriteAllText('%~dp0obj\evc4_arm\install_evc4.cpp', $t, [Text.Encoding]::GetEncoding(1251))" || goto fail
"%EVC4%\bin\clarm.exe" /nologo /O2 /W3 /TP /c obj\evc4_arm\install_evc4.cpp /Foobj\evc4_arm\ ^
  /DUNICODE /D_UNICODE /DUNDER_CE=0x420 /D_WIN32_WCE=0x420 /DWINCE /DARM /D_ARM_ /DARMV4 /DNDEBUG ^
  /I"%~dp0." /I"%SDK%\Include\Armv4" || goto fail
set PATH=%EVC4%\bin;%PATH%
"%EVC4%\bin\link.exe" /nologo /SUBSYSTEM:WINDOWSCE,4.20 /MACHINE:ARM /OUT:"%OUT%\VestaHomeInstall.exe" ^
  obj\evc4_arm\install_evc4.obj obj\evc4_arm\vh_payload.res /LIBPATH:"%SDK%\Lib\Armv4" ^
  coredll.lib corelibc.lib /NODEFAULTLIB:oldnames.lib || goto fail

echo === OK
exit /b 0
:no_evc
echo No eVC4 ARM compiler or Pocket PC 2003 SDK ("%EVC4%", "%SDK%")
goto fail
:no_rc
echo No Visual Studio 2019 Build Tools for rc.exe ("%VS2019%")
goto fail
:no_proxy
echo No %OUT%\SPIPowerMgr.dll or FLC_BLUETOOTH_SDK.dll - git pull (they are in the repo since 2026-10-26)
:fail
echo === BUILD FAILED
exit /b 1

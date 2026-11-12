@echo off
rem The ub-mode checks (SKILL.md) of VestaTether: the tests with coverage (OpenCppCoverage), crapper (CRAP < 10 per
rem function), dryer (no duplicates ranked 0.8+); "mutate": mutator too (slow, edits files in place while it runs).
rem The tools: ..\..\tools. Needs: pip install lizard; OpenCppCoverage. Exit code: the findings.
setlocal
cd /d "%~dp0"
set TOOLS=..\..\tools
set OCC=%ProgramFiles%\OpenCppCoverage\OpenCppCoverage.exe
set BAD=0
set COVER=1
call .\test_vtether.bat >obj\pc\q_vt.log 2>&1 || (type obj\pc\q_vt.log & echo === test_vtether fails & exit /b 1)
call .\test_adb.bat >obj\pc\q_adb.log 2>&1 || (type obj\pc\q_adb.log & echo === test_adb fails & exit /b 1)
set COVER=
"%OCC%" --quiet --sources "%CD%" --excluded_sources "%CD%\test_vtether.cpp" --excluded_sources "%CD%\test_adb.cpp" ^
  --export_type cobertura:obj\pc\cov_vt.xml -- obj\pc\test_vtether.exe >nul
"%OCC%" --quiet --sources "%CD%" --excluded_sources "%CD%\test_vtether.cpp" --excluded_sources "%CD%\test_adb.cpp" ^
  --export_type cobertura:obj\pc\cov_adb.xml -- obj\pc\test_adb.exe >nul
echo === crapper
python %TOOLS%\crapper.py --cobertura obj\pc\cov_vt.xml vtether.cpp
set /a BAD=%BAD%+%errorlevel%
python %TOOLS%\crapper.py --cobertura obj\pc\cov_adb.xml adb.cpp
set /a BAD=%BAD%+%errorlevel%
echo === dryer
python %TOOLS%\dryer.py vtether.cpp adb.cpp rndis.cpp probe.cpp
set /a BAD=%BAD%+%errorlevel%
if "%1"=="mutate" (
  echo === mutator
  python %TOOLS%\mutator.py --build ".\test_adb.bat build" --test "obj\pc\test_adb.exe" --cobertura obj\pc\cov_adb.xml adb.cpp
  set /a BAD=%BAD%+%errorlevel%
  python %TOOLS%\mutator.py --build ".\test_vtether.bat build" --test "obj\pc\test_vtether.exe" --cobertura obj\pc\cov_vt.xml ^
    --lines %2 vtether.cpp
  set /a BAD=%BAD%+%errorlevel%
)
exit /b %BAD%

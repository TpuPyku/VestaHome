@echo off
rem Run a Ghidra post-script against a program of the SPIPower project.
rem Usage: ghidra_post.bat <Script.java> [args ...]
rem        set PROGRAM=<name> to analyze another program (default SwDnl.exe).
rem Kept as a .bat so script arguments with backslashes survive the host shell.
setlocal
set "JAVA_HOME=C:\Program Files\Java\jdk-21"
if "%PROGRAM%"=="" set "PROGRAM=SwDnl.exe"
call "C:\ghidra_1214\support\analyzeHeadless.bat" "E:\GitHub\VestaMMC\Vesta\SPIPower\ghidra" Main -process "%PROGRAM%" -noanalysis -scriptPath "E:\GitHub\VestaMMC\Vesta\SPIPower\ghidra_scripts" -postScript %*

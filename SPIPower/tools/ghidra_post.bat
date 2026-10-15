@echo off
rem Run a Ghidra post-script against a program of the SPIPower project.
rem Usage: ghidra_post.bat <Script.java> [args ...]
rem        set PROGRAM=<name> to analyze another program (default SwDnl.exe).
rem        set GHIDRA=<ghidra folder> and JAVA_HOME if they are not the defaults below.
rem Kept as a .bat so script arguments with backslashes survive the host shell.
setlocal
if "%JAVA_HOME%"=="" set "JAVA_HOME=C:\Program Files\Java\jdk-21"
if "%GHIDRA%"=="" set "GHIDRA=C:\ghidra_1214"
if "%PROGRAM%"=="" set "PROGRAM=SwDnl.exe"
rem the project and the scripts are next to this folder: SPIPower\ghidra, SPIPower\ghidra_scripts
set "SP=%~dp0.."
call "%GHIDRA%\support\analyzeHeadless.bat" "%SP%\ghidra" Main -process "%PROGRAM%" -noanalysis -scriptPath "%SP%\ghidra_scripts" -postScript %*

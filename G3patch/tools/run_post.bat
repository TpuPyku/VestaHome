@echo off
setlocal
set "JAVA_HOME=C:\Program Files\Java\jdk-21"
call "C:\ghidra_1214\support\analyzeHeadless.bat" "E:\GitHub\YaMaps\Vesta\G3patch\ghidra" G3 -process G3NavHMI.orig.exe -noanalysis -scriptPath "E:\GitHub\YaMaps\Vesta\G3patch\ghidra_scripts" -postScript %*

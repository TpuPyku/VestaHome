@echo off
setlocal
set "JAVA_HOME=C:\Program Files\Java\jdk-21"
call "C:\ghidra_1214\support\analyzeHeadless.bat" "E:\GitHub\YaMaps\Vesta\G3patch\ghidra" G3 -import "E:\GitHub\YaMaps\Vesta\G3patch\orig\G3NavHMI.orig.exe"

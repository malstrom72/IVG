@echo off
setlocal
set DIR=%~dp0
set ROOT=%DIR%..

rem Fail helper label for uniform error handling
rem Usage: any_command || goto :error

node %ROOT%\QuickHashGenCLI.node.js --help > "%DIR%help.log" 2>&1 || goto :error
findstr /R /C:"Usage:" "%DIR%help.log" >nul || goto :error
node %ROOT%\QuickHashGenCLI.node.js -h > "%DIR%help.log" 2>&1 || goto :error
findstr /R /C:"Usage:" "%DIR%help.log" >nul || goto :error
del "%DIR%help.log"

node %ROOT%\QuickHashGenCLI.node.js --seed 1 --tests 100 "%DIR%input1.txt" > "%DIR%out1.c" || goto :error
node %ROOT%\QuickHashGenCLI.node.js --seed 1 --tests 100 --force-eval --eval-test "%DIR%input1.txt" > "%DIR%out1_eval.c" || goto :error
fc "%DIR%out1.c" "%DIR%out1_eval.c" > nul || goto :error
fc "%DIR%out1.c" "%DIR%golden1.c" > nul || goto :error
node %ROOT%\QuickHashGenCLI.node.js --seed 123 --tests 100 "%DIR%input2.txt" > "%DIR%out2.c" || goto :error
fc "%DIR%out2.c" "%DIR%golden2.c" > nul || goto :error
node %ROOT%\QuickHashGenCLI.node.js --seed 10 --tests 50000 "%DIR%input3.txt" > "%DIR%out3.c" || goto :error
fc "%DIR%out3.c" "%DIR%golden3.c" > nul || goto :error
node %ROOT%\QuickHashGenCLI.node.js --seed 7 --tests 1000 "%DIR%input4.txt" > "%DIR%out4.c" || goto :error
fc "%DIR%out4.c" "%DIR%golden4.c" > nul || goto :error
node %ROOT%\QuickHashGenCLI.node.js --seed 11 --tests 2000 "%DIR%input5.txt" > "%DIR%out5.c" || goto :error
fc "%DIR%out5.c" "%DIR%golden5.c" > nul || goto :error
node %ROOT%\QuickHashGenCLI.node.js --seed 13 --tests 100000 "%DIR%input6.txt" > "%DIR%out6.c" || goto :error
fc "%DIR%out6.c" "%DIR%golden6.c" > nul || goto :error
node %ROOT%\QuickHashGenCLI.node.js --seed 17 --tests 500 "%DIR%input7.txt" > "%DIR%out7.c" || goto :error
fc "%DIR%out7.c" "%DIR%golden7.c" > nul || goto :error
node %ROOT%\QuickHashGenCLI.node.js --seed 19 --tests 2000 "%DIR%input8.txt" > "%DIR%out8.c" || goto :error
fc "%DIR%out8.c" "%DIR%golden8.c" > nul || goto :error

del "%DIR%out1.c" "%DIR%out1_eval.c" "%DIR%out2.c" "%DIR%out3.c" "%DIR%out4.c" "%DIR%out5.c" "%DIR%out6.c" "%DIR%out7.c" "%DIR%out8.c"

REM Exercise the README example (default 100000 tests) to ensure it completes
(
echo black
echo silver
echo gray
echo white
echo maroon
echo red
echo purple
echo fuchsia
echo green
echo lime
echo olive
echo yellow
echo navy
echo blue
echo teal
echo aqua
) | node %ROOT%\QuickHashGenCLI.node.js > "%DIR%colors.c" || goto :error
if not exist "%DIR%colors.c" goto :error
for %%A in ("%DIR%colors.c") do if %%~zA LSS 1 goto :error
del "%DIR%colors.c"

echo All tests passed.
goto :eof

:error
exit /b 1

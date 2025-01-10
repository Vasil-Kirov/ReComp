@ECHO OFF

set LIBS=-l..\libs\LLVM-C.lib -lDbghelp 

REM set ASAN=-fsanitize=address
REM set TSAN=-fsanitize=thread CURRENTLY NOT SUPPORTED ON WINDOWS
set ASAN=

pushd bin
cl.exe /nologo /LD ../testdll.c
clang++ -O0 -g -orcp.exe %ASAN% ..\src\Main.cpp -I..\include -I..\src %LIBS% -D_CRT_SECURE_NO_WARNINGS -DDEBUG -mavx -Wall
popd


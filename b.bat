@ECHO OFF


set ASAN=-fsanitize=address
REM set ASAN=

pushd bin
cl.exe /nologo /LD ../testdll.c
clang++ %ASAN% --debug ..\src\Main.cpp -I..\include -I..\src -l..\libs\*.lib -lDbghelp -D_CRT_SECURE_NO_WARNINGS -DDEBUG -mavx -Wall
popd


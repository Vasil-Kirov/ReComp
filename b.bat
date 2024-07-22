@ECHO OFF


pushd bin
cl.exe /nologo /LD ../testdll.c
clang++ --debug ..\src\Main.cpp -I..\include -I..\src -l..\libs\*.lib -lDbghelp -D_CRT_SECURE_NO_WARNINGS -DDEBUG -mavx -Wall
popd


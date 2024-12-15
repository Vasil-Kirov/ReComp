@ECHO OFF


set ASAN=-fsanitize=address
REM set TSAN=-fsanitize=thread CURRENTLY NOT SUPPORTED ON WINDOWS
REM set ASAN=

pushd bin
cl.exe /nologo /LD ../testdll.c
clang++ -O0 -g -orcp.exe %TSAN% %ASAN% ..\src\Main.cpp -I..\include -I..\src -l..\libs\*.lib -lDbghelp -D_CRT_SECURE_NO_WARNINGS -DDEBUG -mavx -Wall
popd


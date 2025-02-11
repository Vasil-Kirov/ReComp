@ECHO OFF

set LIBS=-l..\libs\dyncall_s.lib -l..\libs\LLVM-C.lib -lDbghelp 

REM set ASAN=-fsanitize=address
REM set TSAN=-fsanitize=thread CURRENTLY NOT SUPPORTED ON WINDOWS
set ASAN=

set FLAGS=

if "%1" == "rel" (
	echo "foo"
) else if "%1" == "san" (
	echo "ss"
) else (
	echo "bar"
)

pushd bin
cl.exe /nologo /LD ../testdll.c
clang++ -O0 -g -orcp.exe %ASAN% ..\src\Main.cpp -I..\include -I..\src %LIBS% -D_CRT_SECURE_NO_WARNINGS -DDEBUG -mavx -Wall
popd


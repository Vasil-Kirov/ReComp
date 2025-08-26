@ECHO OFF

set LIBS=-l..\libs\dyncall_s.lib -l..\libs\LLVM-C.lib -lDbghelp -lAdvapi32.lib -lOle32.lib -lMincore.lib

:: set ASAN=-fsanitize=address
:: set TSAN=-fsanitize=thread CURRENTLY NOT SUPPORTED ON WINDOWS
set ASAN=

set IGNORE_WARNINGS=-Wno-sign-compare -Wno-missing-field-initializers 
set FLAGS=-std=c++17 -Wall -Wextra %IGNORE_WARNINGS%

if "%1" == "rel" (
	set FLAGS=%FLAGS% -O3
) else if "%1" == "san" (
	set FLAGS=%FLAGS% -fsanitize=address -g -O0
) else (
	set FLAGS=%FLAGS% -O0 -g
)

echo %FLAGS%

pushd bin
:: cl.exe /nologo /LD ../testdll.c
clang++ %FLAGS% -orcp.exe ..\src\Main.cpp -I..\include -I..\src %LIBS% -D_CRT_SECURE_NO_WARNINGS -DDEBUG -mavx -Wall
popd


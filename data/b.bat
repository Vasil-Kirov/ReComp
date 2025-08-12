@ECHO OFF

:: --vmdll ..\bin\testdll.dll 

..\bin\rcp.exe build.rcp %1 %2 %3 --link kernel32.lib
REM LINK pass.obj /NOLOGO /DEFAULTLIB:LIBCMT /ENTRY:mainCRTStartup
REM LINK "!internal.obj" main.obj file2.obj /NOLOGO /DEFAULTLIB:LIBCMT /ENTRY:mainCRTStartup /OUT:a.exe


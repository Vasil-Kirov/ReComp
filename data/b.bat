@ECHO OFF
..\bin\rcp.exe build.rcp %1
REM LINK pass.obj /NOLOGO /DEFAULTLIB:LIBCMT /ENTRY:mainCRTStartup
LINK "!internal.obj" main.obj file2.obj /NOLOGO /DEFAULTLIB:LIBCMT /ENTRY:mainCRTStartup /OUT:a.exe


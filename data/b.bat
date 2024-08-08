@ECHO OFF
..\bin\rcp.exe build.rcp %1
LINK pass.obj /NOLOGO /DEFAULTLIB:LIBCMT /ENTRY:mainCRTStartup


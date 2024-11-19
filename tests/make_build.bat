@ECHO OFF

REM mkdir %1
DEL %1\build.rcp


(
echo build
echo #import compile as c
echo.
echo compile :: fn^(^) -^> c.CompileInfo {
echo     out := c.CompileInfo {
echo         files = []string { "%1.rcp" },
echo         opt = 0,
echo         flags = @u32 c.CompileFlag.SanAddress,
echo     };
echo     return out;
echo }
) > %1\build.rcp

REM (
REM echo main
REM echo.
REM echo main :: fn^(^) -^> i32 {
REM echo 	return 0;
REM echo }
REM ) > %1/%1.rcp


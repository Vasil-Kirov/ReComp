@ECHO OFF

mkdir %1

(
echo build
echo #import compile as c
echo.
echo compile :: fn^(^) -^> c.CompileInfo {
echo     out := c.CompileInfo {
echo         files = []string { "%1.rv" },
echo         opt = 0,
echo         flags = @u32 c.CompileFlag.SanAddress,
echo     };
echo     return out;
echo }
) > %1\build.rv

(
echo main
echo.
echo main :: fn^(^) -^> i32 {
echo 	return 0;
echo }
) > %1/%1.rv


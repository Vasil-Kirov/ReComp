@ECHO OFF

mkdir %1

(
echo build
echo.
echo compile :: fn^(out: *CompileInfo^) {
echo 	to_fill := *out;
echo 	to_fill.files[0] = c"%1.rcp";
echo.
echo 	to_fill.file_count = 1;
echo.
echo 	*out = to_fill;
echo }
) > %1/build.rcp

(
echo main
echo.
echo main :: fn^(^) -^> i32 {
echo 	return 0;
echo }
) > %1/%1.rcp


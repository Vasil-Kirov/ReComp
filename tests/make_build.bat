@ECHO OFF

mkdir %1

echo build  ^
 ^
compile :: fn(out: *CompileInfo) { ^
	to_fill := *out; ^
 ^
	to_fill.files[0] = c"%1.rcp"; ^
 ^
	to_fill.file_count = 1; ^
 ^
	*out = to_fill; ^
} > %1/build.rcp



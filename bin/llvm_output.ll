define i8 @x() {
block_0: 

sext untyped integer  10 to i32
%2 = alloca i32
store i32 %1, ptr %2

sext untyped integer  10 to i16
%5 = alloca i16
store i16 %4, ptr %5
%6 = load i16, ptr %5
sext i16 %6 to i32
%8 = load i32, ptr %2
%9 = add i32 %7, %8
%10 = alloca i32
store i32 %9, ptr %10
%11 = load i32, ptr %10
sext i32 %11 to i64
%13 = alloca i64
store i64 %12, ptr %13
%14 = load i64, ptr %13
ret i64 %14
}
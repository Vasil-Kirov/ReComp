@ECHO OFF


pushd bin
clang --debug ..\src\Main.cpp -D_CRT_SECURE_NO_WARNINGS -DDEBUG -mavx
popd


#!/bin/bash

DEFINES="-D_CRT_SECURE_NO_WARNINGS -DDEBUG -DCM_LINUX"
LIBS=$(llvm-config --ldflags --libs)
LIBS+=' -ldyncall_s'


if [[ "$1" == "rel" ]]
then
	FLAGS='-O3'
else

	FLAGS='-fsanitize=address,undefined --debug -O0'
fi

#DO_ASAN=''

#export ASAN_OPTIONS=detect_leaks=0

echo $FLAGS

pushd bin
clang++ $DO_ASAN -rdynamic -orcp $FLAGS -lz ../src/Main.cpp -I../include -I../src $LIBS $DEFINES -mavx -Wall -Wno-format
popd



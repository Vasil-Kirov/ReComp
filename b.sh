#!/bin/bash

DEFINES="-D_CRT_SECURE_NO_WARNINGS -DDEBUG -DCM_LINUX"
LIBS=$(llvm-config --ldflags --libs)
pushd bin
clang++ -orcp --debug -lz ../src/Main.cpp -I../include -I../src $LIBS $DEFINES -mavx -Wall -Wno-format
popd



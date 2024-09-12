#!/bin/bash

DEFINES="-D_CRT_SECURE_NO_WARNINGS -DDEBUG -DCM_LINUX"
LIBS=$(find /usr/lib64 -name 'libLLVM*' | xargs -0 echo)
pushd bin
clang++ -orcp --debug ../src/Main.cpp -I../include -I../src $LIBS $DEFINES -mavx -Wall -Wno-format
popd



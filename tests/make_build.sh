#!/bin/bash

# Create the directory
mkdir "$1"

# Create the build.rv file
cat << EOF > "$1/build.rv"
build
#import compile as c

compile :: fn() -> c.CompileInfo {
    out := c.CompileInfo {
        files = []string { "$1.rv" },
        opt = 0,
        flags = @u32 c.CompileFlag.SanAddress,
    };
    return out;
}
EOF

# Create the main rcp file
cat << EOF > "$1/$1.rv"
main

main :: fn() -> i32 {
    return 0;
}
EOF


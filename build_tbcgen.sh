#!/bin/bash
cd "$(dirname "$0")"
clang-3.5	-Wall -std=c99 -fPIC -Os tagha_tbcgen.c -shared -o libtagha_tbcgen_clang.so
gcc			-Wall -std=c99 -fPIC -Os tagha_tbcgen.c -shared -o libtagha_tbcgen_gcc.so
#!/usr/bin/env bash

cd ..
mkdir -p 842-build
cd 842-build
cp ../sqlite/Makefile.linux-gcc Makefile
cp ../sqlite/run-842-benchmarks.py run-842-benchmarks.py
cp ../sqlite/libjit.a libjit.a

make clean

# build no-JIT sqlite shell
make
cp sqlite3 sqlite3-no-jit

make clean

# build JIT sqlite shell
make OPTS='-DSQLITE_JIT'
cp sqlite3 sqlite3-jit

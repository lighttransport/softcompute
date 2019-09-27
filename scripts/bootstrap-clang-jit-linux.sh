#!/bin/bash

LLVM_CLANG_DIST=$HOME/local/clang+llvm-8.0.0-x86_64-linux-gnu-ubuntu-16.04/

rm -rf build
mkdir build

cd build

LIBCLANG_DIR=${LLVM_CLANG_DIST}/lib/cmake/clang cmake \
  -DCMAKE_C_COMPILER=${LLVM_CLANG_DIST}/bin/clang \
  -DCMAKE_CXX_COMPILER=${LLVM_CLANG_DIST}/bin/clang++ \
  -DWITH_JIT=On \
  -DLLVM_DIR=${LLVM_CLANG_DIST}/lib/cmake/llvm \
  -DLIBCXX_INCLUDE_DIR=${LLVM_CLANG_DIST}/include/c++/v1 \
  -DSANITIZE_ADDRESS=On \
  ..

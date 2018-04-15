#!/bin/bash

LLVM_CLANG_DIST=$HOME/local/clang+llvm-6.0.0

rm -rf build
mkdir build
cmake \
  -DCMAKE_C_COMPILER=${LLVM_CLANG_DIST}/bin/clang \
  -DCMAKE_CXX_COMPILER=${LLVM_CLANG_DIST}/bin/clang++ \
  -DWITH_JIT=On \
  -DLLVM_DIR=${LLVM_CLANG_DIST}/lib/cmake/llvm \
  -DLIBCXX_INCLUDE_DIR=${LLVM_CLANG_DIST}/include/c++/v1 \
  -DSANITIZE_ADDRESS=On \
  -Bbuild \
  -H.

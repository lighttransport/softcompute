@rem Assume Ninja is installed on your system

@rem Set path to llvm-mingw in env var.
set LLVM_MINGW_DIR=h:/local/llvm-mingw-20230427-ucrt-x86_64/

set builddir=build-llvm-mingw

rmdir /s /q %builddir%
mkdir %builddir%

cmake ^
  -DCMAKE_TOOLCHAIN_FILE=cmake/llvm-mingw-win64.cmake ^
  -G "Ninja" ^
  -DCMAKE_VERBOSE_MAKEFILE=1 ^
  -B %builddir% ^
  -S .


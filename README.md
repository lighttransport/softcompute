# SoftCompute, CPU JIT execution of SPIR-V compute shader execution system.

## Purpose

* Debug compute shader more easily.
* Run compute shader where no compute shader capable OpenGL device is available.
* Currently SoftCompute only could be able to run very simple compute shader.

## Requirements

* Premake5 https://premake.github.io/download.html
* clang/LLVM 3.8+ http://llvm.org/releases/download.html
  * GNU STL or libc++ depending on your clang/LLVM build configuration
  * At least 3.8 prebuilt package for CentOS6 and El Capitan confirmed working
* glslang http://glm.g-truc.net/0.9.7/index.html
* SPIR-Cross https://github.com/KhronosGroup/SPIRV-Cross
* glm 0.9.7.4 or later


## Build on Linux or MacOSX

    $ premake5 gmake

Or you can explicitly specify path to `llvm-config` with

    $ premake5 --llvm-config=/path/to/llvm-config gmake

Then,

    $ export CXX=clang++
    $ make

## Setup and how to run

Put `glm` directory to this directory(or create a symlink).

    $ ls
    glm
    spirv_cross
    bin
    src
    ... 

Create cpp shader from GLSL using `glslangValidator` and `spirv-cross`

    $ glslangValidator -V shaders/ao.comp
    $ spirv-cross --output ao.cc --cpp comp.spv

Then,

    $ ./bin/softcompute ao.cc

### Note

You may manually edit C/C++ header path in `src/jit-engine.cc`

## License

* SoftCompute is licensed under Apache 2.0 License.

### Third party license

* spirv_cross header files are licensed under Apache 2.0 License.
* src/OptionParser is licensed under MIT License.
* src/stb_image_write.h is public domain license.


## Limitation

Only support simple compute shader at this time.

## TODO

* [ ] Read header path from file or else
* [ ] Support various shader type.
* [ ] Flexible shader value binding.
* [ ] Debugger support.
* [ ] GUI?
* [ ] Windows support.
* [ ] Interactive edit & run.
* [ ] Multi-threaded execution.


# SoftCompute, CPU JIT execution of SPIR-V shader execution system.

## Purpose

* Debug compute shader more easily.
* Run compute shader where no compute shader capable OpenGL device is not available.

## Requirements

* Premake5
* clang/LLVM 3.8+
  * gnu STL or libc++ depending on your clang/LLVM build configuration
* C++-11 compiler
* glslang http://glm.g-truc.net/0.9.7/index.html
* SPIR-Cross
* glm 0.9.7.4 or later


## Build on Linux or MacOSX

    $ premake5 gmake

## How to run

Put `glm` files to this directory(or create a symlink).

    $ ls
    glm
    spirv_cross
    bin
    src
    ... 

Create cpp shader from GLSL using `glslangValidator` and `spirv-cross`

    $ glslangValidator -V shaders/twice.comp
    $ spirv-cross --output twice-shader.cc --cpp comp.spv

Then,

    $ ./bin/softcompute twice-shader.cc

### Note

You may manually edit C/C++ header path in `src/jit-engine.cc`


## Limitation


Only support simple compute shader at this time.

## TODO

* [ ] Support various shader type.
* [ ] Flexible shader value binding.
* [ ] Debugger support.
* [ ] GUI?
* [ ] Windows support.


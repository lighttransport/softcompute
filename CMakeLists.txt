cmake_minimum_required(VERSION 3.5.1)

project(SoftCompute)

# [Build options] -------------------------------------------------------
option(WITH_OPENMP "Build with OpenMP support" OFF)
option(WITH_JIT "Build with LLVM/clang JIT support" OFF)
option(LIBCXX_INCLUDE_DIR "Path to libcxx headers)" "/usr/include/c++/v1")
# -----------------------------------------------------------------------

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})
find_package(Sanitizers) # Address sanitizer.

if (WITH_JIT)

  # Enable PIC
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)

  # Search ClangConfig.cmake from LIBCLANG_DIR environment variable.
  find_package(Clang REQUIRED PATHS $ENV{LIBCLANG_DIR})
  set(CLANG_LIBS libclang clangCodeGen)
  include_directories(${CLANG_INCLUDE_DIRS})

  #find_package(LLVM REQUIRED PATHS $ENV{LLVM_DIR} CONFIG NO_CMAKE_BUILDS_PATH)

  ##
  ## libclang settings. based on CMakeLists.txt from Oclgrind.
  ##
  #set (CLANG_ROOT " " CACHE PATH "Root of Clang installation")
  #if (NOT ${CLANG_ROOT} STREQUAL " ")
  #  include_directories("${CLANG_ROOT}/include")
  #  link_directories("${CLANG_ROOT}/lib")
  #  set(CMAKE_REQUIRED_INCLUDES
  #      "${CMAKE_REQUIRED_INCLUDES};${CLANG_ROOT}/include")
  #endif()

  ## Check for Clang headers
  #unset(CLANG_HEADER CACHE)
  #find_path(CLANG_HEADER "clang/CodeGen/CodeGenAction.h"
  #          PATHS "${CLANG_ROOT}/include" "${LLVM_INCLUDE_DIRS}"
  #          NO_DEFAULT_PATH)
  #find_path(CLANG_HEADER "clang/CodeGen/CodeGenAction.h")
  #if ("${CLANG_HEADER}" STREQUAL "CLANG_HEADER-NOTFOUND")
  #  message(FATAL_ERROR "Clang headers not found (set CLANG_ROOT)")
  #endif()

  ## Check for Clang libraries
  #unset(CLANG_LIB CACHE)
  #find_library(CLANG_LIB "clangFrontend"
  #             PATHS "${CLANG_ROOT}/lib" "${LLVM_LIBRARY_DIRS}"
  #             NO_DEFAULT_PATH)
  #find_library(CLANG_LIB "clangFrontend")
  #if ("${CLANG_LIB}" STREQUAL "CLANG_LIB-NOTFOUND")
  #  message(FATAL_ERROR "Clang libraries not found (set CLANG_ROOT)")
  #endif()


  #set(CLANG_LIBS clangFrontend clangSerialization clangDriver clangCodeGen
  #               clangParse clangSema clangAnalysis clangEdit clangAST clangLex clangBasic)

  #add_definitions(${LLVM_DEFINITIONS})
  #include_directories(${LLVM_INCLUDE_DIRS})
  #link_directories(${LLVM_LIBRARY_DIRS})

  # list up LLVM libraries for linking.
  llvm_map_components_to_libnames(LLVM_LIBS all)
  #llvm_map_components_to_libnames(LLVM_LIBS
  #  bitreader bitwriter core coverage instrumentation ipo irreader
  #  mcjit coroutines native lto mcparser objcarcopts option target support)

  #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${LLVM_COMPILE_FLAGS}")

  if (NOT "${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-strict-aliasing")
  endif()

  # Use libc++
  #set(CMAKE_EXE_LINKER_FLAG "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++ -lc++ -lc++abi")
  #add_definitions("-nostdinc++")

  message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
  message(STATUS "Using LLVMConfig.cmake in : ${LLVM_DIR}")
  message(STATUS "LLVM_COMPILE_FLAGS        : ${LLVM_COMPILE_FLAGS}")
  message(STATUS "LLVM_INCLUDE_DIRS         : ${LLVM_INCLUDE_DIRS}")
  message(STATUS "LLVM_LIBRARY_DIRS         : ${LLVM_LIBRARY_DIRS}")
  message(STATUS "CLANG_LIBS                : ${CLANG_LIBS}")
  message(STATUS "LLVM_LIB                  : ${LLVM_LIBS}")

  include_directories(${LLVM_INCLUDE_DIRS})
  #include_directories(${LIBCLANG_INCLUDE_DIRS})
  #include_directories(${LIBCXX_INCLUDE_DIR})

  # libc++
  #set(SOFTCOMPUTE_JIT_LIBS ${CLANG_LIBS} ${LLVM_LIBS} c++ c++abi)

  set(SOFTCOMPUTE_JIT_LIBS ${CLANG_LIBS} ${LLVM_LIBS})

  add_definitions("-DSOFTCOMPUTE_ENABLE_JIT")
endif (WITH_JIT)


# [glslang]
# Disable some build optiosn for glslang
set(SKIP_GLSLANG_INSTALL OFF CACHE INTERNAL "" FORCE)
set(ENABLE_GLSLANG_BINARIES OFF CACHE INTERNAL "Builds glslangValidator and spirv-remap" FORCE)
set(ENABLE_HLSL OFF CACHE INTERNAL "" FORCE)
add_subdirectory(${CMAKE_SOURCE_DIR}/third_party/glslang)

# threads
find_package(Threads)

if (WITH_OPENMP)
  find_package(OpenMP)
  if (OPENMP_FOUND)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
  endif(OPENMP_FOUND)
endif()

# Add custom build type DebugOpt
message("* Adding build types...")
IF (MSVC)
SET(CMAKE_CXX_FLAGS_DEBUGOPT
    "-DDEBUG /DEBUG /O2"
    CACHE STRING "Flags used by the C++ compiler during coverage builds."
    FORCE )
SET(CMAKE_C_FLAGS_DEBUGOPT
    "-DDEBUG /DEBUG /O2"
    CACHE STRING "Flags used by the C compiler during coverage builds."
    FORCE )
ELSE () # Assume gcc
SET(CMAKE_CXX_FLAGS_DEBUGOPT
    "-g -O2 -fno-omit-frame-pointer"
    CACHE STRING "Flags used by the C++ compiler during coverage builds."
    FORCE )
SET(CMAKE_C_FLAGS_DEBUGOPT
    "-g -O2 -fno-omit-frame-pointer"
    CACHE STRING "Flags used by the C compiler during coverage builds."
    FORCE )
ENDIF()

SET(CMAKE_EXE_LINKER_FLAGS_DEBUGOPT
    ""
    CACHE STRING "Flags used for linking binaries during coverage builds."
    FORCE )
SET(CMAKE_SHARED_LINKER_FLAGS_DEBUGOPT
    ""
    CACHE STRING "Flags used by the shared libraries linker during coverage builds."
    FORCE )
MARK_AS_ADVANCED(
    CMAKE_CXX_FLAGS_DEBUGOPT
    CMAKE_C_FLAGS_DEBUGOPT
    CMAKE_EXE_LINKER_FLAGS_DEBUGOPT
    CMAKE_SHARED_LINKER_FLAGS_DEBUGOPT )

IF(NOT CMAKE_BUILD_TYPE)
   SET(CMAKE_BUILD_TYPE Release
       CACHE STRING "Choose the type of build : None Debug Release RelWithDebInfo MinSizeRel DebugOpt."
       FORCE)
ENDIF(NOT CMAKE_BUILD_TYPE)
message("* Current build type is : ${CMAKE_BUILD_TYPE}")

# C++11
set (CMAKE_CXX_STANDARD 11)

# Sanitizer
find_package(Sanitizers)

# [includes]
include_directories(
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/third_party/SPIRV-Cross/include
    ${CMAKE_SOURCE_DIR}/third_party/SPIRV-Cross
    ${CMAKE_SOURCE_DIR}/third_party/glm
    ${CMAKE_SOURCE_DIR}/third_party/spdlog/include
    ${CMAKE_SOURCE_DIR}/third_party/glslang
    ${CMAKE_SOURCE_DIR}/third_party/json/include
    ${CMAKE_SOURCE_DIR}/src
)

if (WITH_JIT)
  set (SOFTCOMPUTE_ENGINE_SOURCE
      ${CMAKE_SOURCE_DIR}/src/jit-engine.cc
      )
else (WITH_JIT)
  set (SOFTCOMPUTE_ENGINE_SOURCE
      ${CMAKE_SOURCE_DIR}/src/dll-engine.cc
      )
endif(WITH_JIT)

set (SOFTCOMPUTE_CORE_SOURCE
    ${SOFTCOMPUTE_ENGINE_SOURCE}
    ${CMAKE_SOURCE_DIR}/src/softgl.cc
    )
add_library(softcompute_core SHARED ${SOFTCOMPUTE_CORE_SOURCE})

set (SPIRV_CROSS_SOURCE
    ${CMAKE_SOURCE_DIR}/third_party/SPIRV-Cross/spirv_cross.cpp
    ${CMAKE_SOURCE_DIR}/third_party/SPIRV-Cross/spirv_cfg.cpp
    ${CMAKE_SOURCE_DIR}/third_party/SPIRV-Cross/spirv_cpp.cpp
    ${CMAKE_SOURCE_DIR}/third_party/SPIRV-Cross/spirv_glsl.cpp
    )
add_library(spirv_cross SHARED ${SPIRV_CROSS_SOURCE})


# [Executable]
add_executable ( softcompute
    ${CMAKE_SOURCE_DIR}/src/main.cc
    ${CMAKE_SOURCE_DIR}/src/OptionParser.cpp
    ${SOFTCOMPUTE_EXTRA_SOURCE}
)

target_link_libraries( softcompute PRIVATE
    softcompute_core
    spirv_cross
    glslang
    SPIRV
    ${SOFTCOMPUTE_EXT_LIBS}
    ${CMAKE_THREAD_LIBS_INIT}
    ${CMAKE_DL_LIBS}
    ${SOFTCOMPUTE_JIT_LIBS})


# Increase warning level for clang.
target_compile_options(softcompute_core PRIVATE $<$<CXX_COMPILER_ID:Clang>: -Weverything -Werror -Wno-padded -Wno-c++98-compat-pedantic -Wno-documentation -Wno-documentation-unknown-command>)

add_sanitizers(softcompute)

# [VisualStudio]
if (WIN32)
  # Set `softcompute` as a startup project for VS IDE
  if (CMAKE_VERSION VERSION_GREATER 3.6.0)
    set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT softcompute)
  endif ()

  # For easier debugging in VS IDE(cmake 3.8.0 or later required)
  if (CMAKE_VERSION VERSION_GREATER 3.8.0)
    # Set working directory to $lucia git repo root.
    set_target_properties(softcompute PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
  endif ()
endif ()


# [cpplint]
# TODO(LTE):
# ADD_CUSTOM_TARGET(cpplint
#   python
#   ${PROJECT_SOURCE_DIR}/tools/cpplint.py
#   --filter=-runtime/rtti,-build/include_order
#   ${PROJECT_SOURCE_DIR}/src/*.cc
# )

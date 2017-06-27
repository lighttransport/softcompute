#include <cstdio>
#include <cstdlib>

#include "softgl.h"

#define CATCH_CONFIG_MAIN
#include "catch.hpp"

using namespace softgl;

TEST_CASE("create_program", "[program]") {
  softgl::InitSoftGL(); 

  GLuint id = glCreateProgram();
  REQUIRE(id > 0);

  softgl::ReleaseSoftGL(); 
}

TEST_CASE("ininitialize", "[init]") {
  softgl::InitSoftGL(); 

  REQUIRE(1 == 1);

  softgl::ReleaseSoftGL(); 
}


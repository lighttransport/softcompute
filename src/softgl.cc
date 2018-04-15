#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#endif

#include "softgl.h"

#include <algorithm>
#include <cassert>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#include <sys/stat.h>
#include <unistd.h>
#endif


#include "spirv_cpp.hpp"
#include "spirv_cross/external_interface.h"
#include "spirv_cross/internal_interface.hpp"
#include "spirv_glsl.hpp"

#include "spdlog/spdlog.h"

// glslang
#include "glslang/Public/ShaderLang.h"
#include "SPIRV/GlslangToSpv.h"

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#ifdef SOFTCOMPUTE_ENABLE_JIT
#include "jit-engine.h"
#else
#include "dll-engine.h"
#endif

namespace softgl {

typedef struct spirv_cross_interface *(*spirv_cross_get_interface_fn)();

const int kMaxUniforms = 64;
const int kMaxBuffers = 64;
const int kMaxPrograms = 64;
const int kMaxShaders = 64;

struct Buffer {
  std::vector<uint8_t> data;
  bool deleted;
  char pad[7];

  Buffer() { deleted = true; }
};

struct Accessor {
  size_t offset;
  size_t size;
  uint32_t buffer_index;
  bool assigned;
  char pad[7];
  int pad1;

  Accessor() {
    offset = 0;
    size = 0;
    buffer_index = 0;
    assigned = false;
  }
};

struct Uniform {
  union {
    float fv[4];
    int iv[4];
  };

  int type;
  int count;
};

struct Program {
  std::vector<uint32_t> shaders;  // List of attached shaders

  bool deleted;
  bool linked;
  char buf[6];

  std::shared_ptr<spirv_cross::CompilerCPP> cpp;
  std::shared_ptr<softcompute::ShaderInstance> instance;
  std::shared_ptr<spirv_cross_shader_t> shader;

  Program() {
    deleted = true;
    linked = false;
    //cpp = nullptr;
    //instance = nullptr;
    //shader = nullptr;
  }

  //~Program() {
    //delete instance;
    //delete shader;
  //}

  bool FindUniformLocation(const char *uniform_name, int *idx) {
    if (!cpp) {
      return false;
    }

    const uint32_t num_ids = cpp->get_current_id_bound();
    for (uint32_t i = 0; i < num_ids; i++) {
      const std::string &name = cpp->get_name(i);
      // LOG_F(INFO, "name[%d] = %s", i, name.c_str());

      if (name.compare(uniform_name) == 0) {
        // LOG_F(INFO, "Bingo!");
        if (idx) {
          (*idx) = int(i);
        }
        return true;
      }
    }

    return false;
  }
};

struct Shader {
  std::vector<uint32_t> binary;  // Shader binary input(Assume SPIR-V binary)
  std::string source;            // Shader source input

  bool deleted;
  char buf[7];

  Shader() { deleted = true; }
};

class SoftGLContext {
 public:
  SoftGLContext() : error_(GL_NO_ERROR) {
    // 0th index is reserved.
    programs.resize(kMaxPrograms + 1);
    buffers.resize(kMaxBuffers + 1);
    shaders.resize(kMaxShaders + 1);
    shader_storage_buffer_accessor.resize(kMaxBuffers + 1);
    uniform_buffer_accessor.resize(kMaxBuffers + 1);

    uniforms.resize(kMaxUniforms);

    active_buffer_index = 0;
    active_program = 0;
  }

  ~SoftGLContext() {}

  void SetJITCompilerOptions(const std::string &option_string) {
    jit_compile_options_ = option_string;
  }

  std::string GetJITCompileOptions() const {
    return jit_compile_options_;
  }

  void SetGLError(const GLenum error) { error_ = error; }

  Program &GetProgram(uint32_t idx) {
    if (idx == 0) {
      // ABORT_F("Program index is zero");
    }

    if (idx >= uint32_t(programs.size())) {
      // ABORT_F("Invalid Program index");
    }

    return programs[idx];
  }

  uint32_t active_buffer_index;
  uint32_t active_program;

  std::vector<Accessor> shader_storage_buffer_accessor;
  std::vector<Accessor> uniform_buffer_accessor;
  std::vector<Uniform> uniforms;
  std::vector<Buffer> buffers;
  std::vector<Program> programs;
  std::vector<Shader> shaders;


 private:
  std::string jit_compile_options_;

  GLenum error_;
};

static SoftGLContext *gCtx = nullptr;

// --------------------------------------------------------------------------------
// Simple offline complilation functions.

/// Generate unique filename.
static std::string GenerateUniqueFilename() {
  char basename[] = "softcompute_XXXXXX";

#if defined(_WIN32)

  assert(strlen(basename) < 1023);

  char buf[1024];
  strncpy(buf, basename, strlen(basename));
  buf[strlen(basename)] = '\0';

  size_t len = strlen(buf) + 1;
  int err =
      _mktemp_s(buf, len);  // prefix will be overwritten with actual filename
  if (err != 0) {
    std::cerr << "Failed to create unique filename.\n";
    return std::string();
  }

  printf("DBG: Unique name: %s", buf);

  std::string name = std::string(buf);

#else
  int fd =
      mkstemp(basename);  // prefix will be overwritten with actual filename
  if (fd == -1) {
    std::cerr << "Failed to create unique filename.\n";
    return std::string();
  }
  close(fd);
  int ret = unlink(basename);
  if (ret == -1) {
    std::cerr << "Failed to delete file: " << basename << std::endl;
  }

  std::string name = std::string(basename);

#endif

  return name;
}

#if 0
static bool exec_command(std::vector<std::string> *outputs,
                         const std::string &cmd) {
  outputs->clear();

  // This may not be needed, but for the safety.
  // See popen(3) manual for details why calling fflush(nullptr) here
  fflush(nullptr);

#if defined(_WIN32)
  FILE *pfp = _popen(cmd.c_str(), "r");
#else
  FILE *pfp = popen(cmd.c_str(), "r");
#endif

  if (!pfp) {
    std::cerr << "Failed to open pipe." << std::endl;
    perror("popen");

    return false;
  }

  char buf[4096];
  while (fgets(buf, 4095, pfp) != nullptr) {
    // printf("%s", buf);
    outputs->push_back(buf);
  }

#if defined(_WIN32)
  int status = _pclose(pfp);
  if (status == -1) {
    fprintf(stderr, "Failed to close pipe.\n");
    return false;
  }
#else
  int status = pclose(pfp);
  if (status == -1) {
    fprintf(stderr, "Failed to close pipe.\n");
    return false;
  }
#endif

  return true;
}
#endif

static void PutsIfNonEmpty(const char *str)
{
  if (str && str[0]) {
    puts(str);
  }
}

// glsl string -> spirv
static bool compile_glsl_string(const std::string &glsl_input, const std::string &filename, std::vector<uint32_t> *out_spirv) {

  TBuiltInResource resources = {
    /* .MaxLights = */ 32,
    /* .MaxClipPlanes = */ 6,
    /* .MaxTextureUnits = */ 32,
    /* .MaxTextureCoords = */ 32,
    /* .MaxVertexAttribs = */ 64,
    /* .MaxVertexUniformComponents = */ 4096,
    /* .MaxVaryingFloats = */ 64,
    /* .MaxVertexTextureImageUnits = */ 32,
    /* .MaxCombinedTextureImageUnits = */ 80,
    /* .MaxTextureImageUnits = */ 32,
    /* .MaxFragmentUniformComponents = */ 4096,
    /* .MaxDrawBuffers = */ 32,
    /* .MaxVertexUniformVectors = */ 128,
    /* .MaxVaryingVectors = */ 8,
    /* .MaxFragmentUniformVectors = */ 16,
    /* .MaxVertexOutputVectors = */ 16,
    /* .MaxFragmentInputVectors = */ 15,
    /* .MinProgramTexelOffset = */ -8,
    /* .MaxProgramTexelOffset = */ 7,
    /* .MaxClipDistances = */ 8,
    /* .MaxComputeWorkGroupCountX = */ 65535,
    /* .MaxComputeWorkGroupCountY = */ 65535,
    /* .MaxComputeWorkGroupCountZ = */ 65535,
    /* .MaxComputeWorkGroupSizeX = */ 1024,
    /* .MaxComputeWorkGroupSizeY = */ 1024,
    /* .MaxComputeWorkGroupSizeZ = */ 64,
    /* .MaxComputeUniformComponents = */ 1024,
    /* .MaxComputeTextureImageUnits = */ 16,
    /* .MaxComputeImageUniforms = */ 8,
    /* .MaxComputeAtomicCounters = */ 8,
    /* .MaxComputeAtomicCounterBuffers = */ 1,
    /* .MaxVaryingComponents = */ 60,
    /* .MaxVertexOutputComponents = */ 64,
    /* .MaxGeometryInputComponents = */ 64,
    /* .MaxGeometryOutputComponents = */ 128,
    /* .MaxFragmentInputComponents = */ 128,
    /* .MaxImageUnits = */ 8,
    /* .MaxCombinedImageUnitsAndFragmentOutputs = */ 8,
    /* .MaxCombinedShaderOutputResources = */ 8,
    /* .MaxImageSamples = */ 0,
    /* .MaxVertexImageUniforms = */ 0,
    /* .MaxTessControlImageUniforms = */ 0,
    /* .MaxTessEvaluationImageUniforms = */ 0,
    /* .MaxGeometryImageUniforms = */ 0,
    /* .MaxFragmentImageUniforms = */ 8,
    /* .MaxCombinedImageUniforms = */ 8,
    /* .MaxGeometryTextureImageUnits = */ 16,
    /* .MaxGeometryOutputVertices = */ 256,
    /* .MaxGeometryTotalOutputComponents = */ 1024,
    /* .MaxGeometryUniformComponents = */ 1024,
    /* .MaxGeometryVaryingComponents = */ 64,
    /* .MaxTessControlInputComponents = */ 128,
    /* .MaxTessControlOutputComponents = */ 128,
    /* .MaxTessControlTextureImageUnits = */ 16,
    /* .MaxTessControlUniformComponents = */ 1024,
    /* .MaxTessControlTotalOutputComponents = */ 4096,
    /* .MaxTessEvaluationInputComponents = */ 128,
    /* .MaxTessEvaluationOutputComponents = */ 128,
    /* .MaxTessEvaluationTextureImageUnits = */ 16,
    /* .MaxTessEvaluationUniformComponents = */ 1024,
    /* .MaxTessPatchComponents = */ 120,
    /* .MaxPatchVertices = */ 32,
    /* .MaxTessGenLevel = */ 64,
    /* .MaxViewports = */ 16,
    /* .MaxVertexAtomicCounters = */ 0,
    /* .MaxTessControlAtomicCounters = */ 0,
    /* .MaxTessEvaluationAtomicCounters = */ 0,
    /* .MaxGeometryAtomicCounters = */ 0,
    /* .MaxFragmentAtomicCounters = */ 8,
    /* .MaxCombinedAtomicCounters = */ 8,
    /* .MaxAtomicCounterBindings = */ 1,
    /* .MaxVertexAtomicCounterBuffers = */ 0,
    /* .MaxTessControlAtomicCounterBuffers = */ 0,
    /* .MaxTessEvaluationAtomicCounterBuffers = */ 0,
    /* .MaxGeometryAtomicCounterBuffers = */ 0,
    /* .MaxFragmentAtomicCounterBuffers = */ 1,
    /* .MaxCombinedAtomicCounterBuffers = */ 1,
    /* .MaxAtomicCounterBufferSize = */ 16384,
    /* .MaxTransformFeedbackBuffers = */ 4,
    /* .MaxTransformFeedbackInterleavedComponents = */ 64,
    /* .MaxCullDistances = */ 8,
    /* .MaxCombinedClipAndCullDistances = */ 8,
    /* .MaxSamples = */ 4,
    /* .limits = */ {
        /* .nonInductiveForLoops = */ 1,
        /* .whileLoops = */ 1,
        /* .doWhileLoops = */ 1,
        /* .generalUniformIndexing = */ 1,
        /* .generalAttributeMatrixVectorIndexing = */ 1,
        /* .generalVaryingIndexing = */ 1,
        /* .generalSamplerIndexing = */ 1,
        /* .generalVariableIndexing = */ 1,
        /* .generalConstantMatrixVectorIndexing = */ 1,
    }};


  glslang::TProgram &program = *new glslang::TProgram;
  glslang::TShader *shader = new glslang::TShader(EShLangCompute); // compute shader

  const char *text[1];
  const char *filename_list[1];
  int count = 1;

  text[0] = glsl_input.c_str();
  filename_list[0] = filename.c_str();

  shader->setStringsWithLengthsAndNames(text, nullptr, filename_list, count);

  //std::vector<std::string> processes; // TODO(LTE)
  //shader->addProcesses(processes);

  // SPIR-V settings.
  shader->setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_0);

  EShMessages messages = EShMsgDefault;

  // TODO(LTE): Includer, preprocess.
  bool compile_ok = shader->parse(&resources, /* version */110, false, messages);

  PutsIfNonEmpty(shader->getInfoLog());
  PutsIfNonEmpty(shader->getInfoDebugLog());

  if (!compile_ok) {
    std::cerr << "Compile failed." << std::endl;
  } else {
    std::cout << "Compile OK." << std::endl;
  }

  program.addShader(shader);

  bool link_ok = program.link(messages);
  if (!link_ok) {
    std::cerr << "Link failed." << std::endl;
  } else {
    std::cout << "Link OK." << std::endl;
  }

  bool ok = false;

  if (compile_ok && link_ok) {

    glslang::SpvOptions spv_options;
    spv_options.disableOptimizer = false;
    spv_options.optimizeSize = false;
    spv::SpvBuildLogger logger;

    std::vector<unsigned int> spirv;

    glslang::GlslangToSpv(*program.getIntermediate(static_cast<EShLanguage>(EShLangCompute)), spirv, &logger, &spv_options);

    if (spirv.size() > 0) {
      (*out_spirv) = spirv;
      ok = true;
    }
  }

#if 0
  // From ResourceLimits.cpp in glslang/StandAlone
  int ret = ShCompile(compiler, &shader_string, 1, nullptr, EShOptNone, &resources, options, /* version */110, false, messages);

  free(shader_string);
#endif

  delete &program;
  delete shader;

  return true;
}


#if 0
// glsl -> spirv
static bool compile_glsl(const std::string &output_filename, bool verbose,
                         const std::string &glsl_filename) {
  //
  // Invoke `glslangValidator`
  //
  std::string binary = "glslangValidator";
  char *binary_path = getenv("GLSLANG_VALIDADOR");
  if (binary_path) {
    binary = binary_path;
    if (verbose) {
      printf("glslangValidator = %s\n", binary.c_str());
    }
  }

  std::stringstream ss;
  ss << binary;
  ss << " -V";
  ss << " -o " << output_filename.c_str();
  ss << " " << glsl_filename;

  std::string cmd;
  cmd = ss.str();
  if (verbose) {
    std::cout << cmd << std::endl;
  }

  std::vector<std::string> outputs;
  bool ret = exec_command(&outputs, cmd);
  if (ret) {
    //
    // Check if compiled cpp code exists.
    //
    std::ifstream ifile(output_filename);
    if (!ifile) {
      std::cerr << "Failed to translate GLSL to SPIR-V" << std::endl;
      return false;
    }
  }

  return true;
}

// spirv file -> c++
static bool compile_spirv(const std::string &output_filename, bool verbose, const std::string &spirv_filename)
{
    //
    // Invoke `spirv-cross`
    //
    std::string spirv_cross = "spirv-cross";
    char *spirv_cross_path = getenv("SPIRV_CROSS");
    if (spirv_cross_path)
    {
        spirv_cross = spirv_cross_path;
        if (verbose)
        {
            printf("spirv-cross = %s\n", spirv_cross.c_str());
        }
    }

    std::stringstream ss;
    ss << spirv_cross;
    ss << " --output " << output_filename.c_str();
    ss << " --cpp";
    ss << " " << spirv_filename;
    ss << " --dump-resources";
    ss << " 2>&1"; // spirv-cross dumps info to stderr. redirect stderr to stdout to catch dump info.

    std::string cmd;
    cmd = ss.str();
    if (verbose)
    {
        std::cout << cmd << std::endl;
    }

    std::vector<std::string> outputs;
    bool ret = exec_command(&outputs, cmd);
    if (ret)
    {
        //
        // Check if compiled cpp code exists.
        //
        std::ifstream ifile(output_filename);
        if (!ifile)
        {
            LOG_F(ERROR, "Failed to translate SPIR-V to C++");
            return false;
        }
        return true;
    }

    return false;
}
#else
// spirv binary file -> c++
static bool compile_spirv_binary(const std::string &output_filename,
                                 bool verbose,
                                 const std::vector<uint32_t> &spirv_binary) {
  std::unique_ptr<spirv_cross::CompilerGLSL> compiler =
      std::unique_ptr<spirv_cross::CompilerGLSL>(
          new spirv_cross::CompilerCPP(spirv_binary));

  std::string code = compiler->compile();

  std::ofstream ofs(output_filename);
  if (!ofs) {
    // LOG_F(ERROR, "Failed to open file for wtire : %s",
    // output_filename.c_str());
    return false;
  }

  ofs << code;

  (void)verbose;

  return true;
}
#endif

#if 0
// c++ -> dll
static bool compile_cpp(const std::string &output_filename,
                        const std::string &options, bool verbose,
                        const std::string &cpp_filename) {
  //
  // Invoke C++ compiler
  //
  std::string cpp = "g++";
  char *cpp_env = getenv("CXX");
  if (cpp_env) {
    cpp = cpp_env;
    if (verbose) {
      printf("cpp = %s\n", cpp.c_str());
    }
  }

  // Assume gcc or clang. Assume mingw on windows.
  std::stringstream ss;
  ss << cpp;
  ss << " -std=c++11";
  ss << " -I./third_party/glm";  // TODO(syoyo): User-supplied path to glm
  ss << " -I./third_party/SPIRV-Cross/include";  // TODO(syoyo): User-supplied
                                                 // path to SPIRV-Cross/include.
  ss << " -o " << output_filename;
#ifdef __APPLE__
  ss << " -flat_namespace";
  ss << " -bundle";
  ss << " -undefined suppress";
#else
  ss << " -shared";
  ss << " -g";
#endif
#ifdef __linux__
  ss << " -fPIC";
#endif
  ss << " " << options;
  ss << " " << cpp_filename;

  std::string cmd;
  cmd = ss.str();
  if (verbose) std::cout << cmd << std::endl;

  std::vector<std::string> outputs;
  bool ret = exec_command(&outputs, cmd);
  if (ret) {
    //
    // Check if compiled dll file exists.
    //
    std::ifstream ifile(output_filename);
    if (!ifile) {
      std::cerr << "Failed to compile C++" << std::endl;
      return false;
    }
  }

  return true;
}
#endif

// -------------------------------------------------------------------

void InitSoftGL() {
  // Pass dummy argc/argv;
  int argc = 1;
  const char *argv[] = {"softgl", nullptr};
  (void)argc;
  (void)argv;

  // loguru::init(argc, const_cast<char **>(argv));
  // LOG_F(INFO, "Initialize SoftGL context");
  gCtx = new SoftGLContext();
}

void ReleaseSoftGL() {
  // LOG_F(INFO, "Relese SoftGL context");
  delete gCtx;
  gCtx = nullptr;
}

static void InitializeGLContext() {
  if (gCtx == nullptr) {
    InitSoftGL();
  }
}

static void SetGLError(GLenum error) {
  if (gCtx) {
    gCtx->SetGLError(error);
  }
}

void SetJITCompilerOptions(const char *option_string) {
  InitializeGLContext();

  if (option_string) {
    gCtx->SetJITCompilerOptions(std::string(option_string));
  }
}


void glUniform1f(GLint location, GLfloat v0) {
  InitializeGLContext();
  if (location < 0) return;
  assert(static_cast<size_t>(location) < gCtx->uniforms.size());

  gCtx->uniforms[static_cast<size_t>(location)].fv[0] = v0;
  gCtx->uniforms[static_cast<size_t>(location)].type = GL_FLOAT;
  gCtx->uniforms[static_cast<size_t>(location)].count = 1;
}

void glUniform2f(GLint location, GLfloat v0, GLfloat v1) {
  InitializeGLContext();
  if (location < 0) return;
  assert(static_cast<size_t>(location) < gCtx->uniforms.size());

  gCtx->uniforms[static_cast<size_t>(location)].fv[0] = v0;
  gCtx->uniforms[static_cast<size_t>(location)].fv[1] = v1;
  gCtx->uniforms[static_cast<size_t>(location)].type = GL_FLOAT;
  gCtx->uniforms[static_cast<size_t>(location)].count = 2;
}

void glUniform3f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2) {
  InitializeGLContext();
  if (location < 0) return;
  assert(static_cast<size_t>(location) < gCtx->uniforms.size());

  gCtx->uniforms[static_cast<size_t>(location)].fv[0] = v0;
  gCtx->uniforms[static_cast<size_t>(location)].fv[1] = v1;
  gCtx->uniforms[static_cast<size_t>(location)].fv[2] = v2;
  gCtx->uniforms[static_cast<size_t>(location)].type = GL_FLOAT;
  gCtx->uniforms[static_cast<size_t>(location)].count = 3;
}

void glUniform4f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2,
                 GLfloat v3) {
  InitializeGLContext();
  if (location < 0) return;
  assert(static_cast<size_t>(location) < gCtx->uniforms.size());

  gCtx->uniforms[static_cast<size_t>(location)].fv[0] = v0;
  gCtx->uniforms[static_cast<size_t>(location)].fv[1] = v1;
  gCtx->uniforms[static_cast<size_t>(location)].fv[2] = v2;
  gCtx->uniforms[static_cast<size_t>(location)].fv[3] = v3;
  gCtx->uniforms[static_cast<size_t>(location)].type = GL_FLOAT;
  gCtx->uniforms[static_cast<size_t>(location)].count = 4;
}

void glUniform1i(GLint location, GLint v0) {
  InitializeGLContext();
  if (location < 0) return;
  assert(static_cast<size_t>(location) < gCtx->uniforms.size());

  gCtx->uniforms[static_cast<size_t>(location)].iv[0] = v0;
  gCtx->uniforms[static_cast<size_t>(location)].type = GL_INT;
  gCtx->uniforms[static_cast<size_t>(location)].count = 1;
}

void glUniform2i(GLint location, GLint v0, GLint v1) {
  InitializeGLContext();
  if (location < 0) return;
  assert(static_cast<size_t>(location) < gCtx->uniforms.size());

  gCtx->uniforms[static_cast<size_t>(location)].iv[0] = v0;
  gCtx->uniforms[static_cast<size_t>(location)].iv[1] = v1;
  gCtx->uniforms[static_cast<size_t>(location)].type = GL_INT;
  gCtx->uniforms[static_cast<size_t>(location)].count = 2;
}

void glUniform3i(GLint location, GLint v0, GLint v1, GLint v2) {
  InitializeGLContext();
  if (location < 0) return;
  assert(static_cast<size_t>(location) < gCtx->uniforms.size());

  gCtx->uniforms[static_cast<size_t>(location)].iv[0] = v0;
  gCtx->uniforms[static_cast<size_t>(location)].iv[1] = v1;
  gCtx->uniforms[static_cast<size_t>(location)].iv[2] = v2;
  gCtx->uniforms[static_cast<size_t>(location)].type = GL_INT;
  gCtx->uniforms[static_cast<size_t>(location)].count = 3;
}

void glUniform4i(GLint location, GLint v0, GLint v1, GLint v2, GLint v3) {
  InitializeGLContext();
  if (location < 0) return;
  assert(static_cast<size_t>(location) < gCtx->uniforms.size());

  gCtx->uniforms[static_cast<size_t>(location)].iv[0] = v0;
  gCtx->uniforms[static_cast<size_t>(location)].iv[1] = v1;
  gCtx->uniforms[static_cast<size_t>(location)].iv[2] = v2;
  gCtx->uniforms[static_cast<size_t>(location)].iv[3] = v3;
  gCtx->uniforms[static_cast<size_t>(location)].type = GL_INT;
  gCtx->uniforms[static_cast<size_t>(location)].count = 4;
}

void glGenBuffers(GLsizei n, GLuint *buffers) {
  InitializeGLContext();
  assert(n >= 0);
  assert(n < kMaxBuffers);
  assert(buffers);

  // Simple linear seach to find available Buffer.
  // Skip 0th index since its reserved.
  int count = 0;
  for (size_t i = 1; i < gCtx->buffers.size(); i++) {
    if (gCtx->buffers[i].deleted) {
      buffers[count] = static_cast<GLuint>(i);
      gCtx->buffers[i].deleted = false;
      count++;

      if (count >= n) break;
    }
  }
}

GLuint glCreateProgram() {
  InitializeGLContext();

  // Simple linear seach to find available Program.
  // Skip 0th index since its reserved.
  for (size_t i = 1; i < gCtx->programs.size(); i++) {
    if (gCtx->programs[i].deleted) {
      gCtx->programs[i].deleted = false;
      return static_cast<GLuint>(i);
    }
  }

  return 0;  // Invalid
}

GLuint glCreateShader(GLenum shader_type) {
  assert(shader_type == GL_COMPUTE_SHADER);
  (void)shader_type;

  // Simple linear seach to find available Shader.
  // Skip 0th index since its reserved.
  for (size_t i = 1; i < gCtx->shaders.size(); i++) {
    if (gCtx->shaders[i].deleted) {
      gCtx->shaders[i].deleted = false;
      return static_cast<GLuint>(i);
    }
  }

  return 0;
}

void glLinkProgram(GLuint program) {
  InitializeGLContext();

  assert(program < kMaxPrograms);

  if (program == 0) {
    return;
  }

  Program &prog = gCtx->programs[program];

  if (prog.deleted) {
    // LOG_F(ERROR, "[SoftGL] Program %d is not created.", program);
    return;
  }

  if (prog.linked) {
    // LOG_F(ERROR, "[SoftGL] Program %d is already linked.", program);
    return;
  }

  // CHECK_F(prog.shaders.size() == 1, "Currently only one shader per program
  // expected, but got %d", int(prog.shaders.size()));

  GLuint shader_idx = prog.shaders[0];

  if (shader_idx == 0) {
    // LOG_F(ERROR, "[SoftGL] Invalid shader attached to the program.");
    return;
  }

  // CHECK_F(shader_idx < gCtx->shaders.size(), "Invalid shader ID %d",
  // shader_idx);

  const Shader &shader = gCtx->shaders[shader_idx];

  if (shader.binary.size() == 0) {
    // LOG_F(ERROR, "[SoftGL] No shader binary assined.");
    return;
  }

  std::string basename = GenerateUniqueFilename();
  std::string spirv_filename = basename + ".spv";
  std::string cpp_filename = basename + ".cc";

#ifdef _WIN32
  std::string dll_filename = basename + ".dll";
#else
  std::string dll_filename = basename + ".so";
#endif

  {
    // Save CPP compiler context of SPIRV-Cross for later use.
    prog.cpp = std::make_shared<spirv_cross::CompilerCPP>(shader.binary);
  }

  {
    bool ret =
        compile_spirv_binary(cpp_filename, /* verbose */ true, shader.binary);
    if (!ret) {
      // ABORT_F("Failed to translate SPIR-V binary to .cpp");
      std::cerr << "Failed to translate SPIR-V binary to .cpp" << std::endl;
    }
  }

#if 0
  {
    std::string cpp_compile_options;  // FIXME(syoyo): Supply compiler options

    // c++ -> dll
    bool ret = compile_cpp(dll_filename, cpp_compile_options,
                           /* verbose */ true, cpp_filename);
    if (!ret) {
      std::cerr << "Failed to compile C++ into dll module." << std::endl;
      return;
    }
  }

  softcompute::ShaderEngine engine;
  std::vector<std::string> search_paths;
  std::string compile_options;

  // LOG_F(INFO, "load dll...");
  prog.instance = std::make_shared<softcompute::ShaderInstance>(*(engine.Compile("comp", /* id */ 0, search_paths,
                                 compile_options, dll_filename)));

  // LOG_F(INFO, "loaded dll...");
  spirv_cross_get_interface_fn interface_fn =
      reinterpret_cast<spirv_cross_get_interface_fn>(
          prog.instance->GetInterfaceFuncPtr());

  struct spirv_cross_interface *interface = interface_fn();
  //fprintf(stderr, "construct: %p\n", interface->construct());
  prog.shader = std::make_shared<spirv_cross_shader_t>(*(interface->construct()));

  // LOG_F(INFO, "linked...");
  prog.linked = true;
#else

  softcompute::ShaderEngine engine;
  std::vector<std::string> search_paths;
  std::string compile_options;

  {
    std::stringstream ss;

    //ss << "-I./third_party/glm ";  // TODO(syoyo): User-supplied path to glm
    ss << "-I./third_party/SPIRV-Cross/include ";  

    ss << gCtx->GetJITCompileOptions() << " ";

    compile_options = ss.str();
  }
    
  prog.instance = std::make_shared<softcompute::ShaderInstance>(*(engine.Compile("comp", /* id */ 0, search_paths, compile_options, cpp_filename)));

  // LOG_F(INFO, "loaded dll...");
  spirv_cross_get_interface_fn interface_fn =
      reinterpret_cast<spirv_cross_get_interface_fn>(
          prog.instance->GetInterfaceFuncPtr());

  struct spirv_cross_interface *interface = interface_fn();
  //fprintf(stderr, "construct: %p\n", interface->construct());
  prog.shader = std::make_shared<spirv_cross_shader_t>(*(interface->construct()));

  // LOG_F(INFO, "linked...");
  prog.linked = true;

#endif
}

void glBindBuffer(GLenum target, GLuint buffer) {
  InitializeGLContext();
  assert((target == GL_SHADER_STORAGE_BUFFER) || (target == GL_UNIFORM_BUFFER));
  (void)target;

  gCtx->active_buffer_index = buffer;
}

void glBindBufferBase(GLenum target, GLuint index, GLuint buffer) {
  (void)target;
  (void)index;
  (void)buffer;

  // LOG_F(ERROR, "TODO");

  return;
}

void glBindBufferRange(GLenum target, GLuint index, GLuint buffer,
                       GLintptr offset, GLsizeiptr size) {
  InitializeGLContext();
  assert((target == GL_SHADER_STORAGE_BUFFER) || (target == GL_UNIFORM_BUFFER));

  assert(index <= kMaxBuffers);
  assert(buffer < gCtx->buffers.size());
  assert(gCtx->buffers[buffer].deleted == false);

  assert(static_cast<size_t>(offset + size) <=
         gCtx->buffers[buffer].data.size());

  if (target == GL_SHADER_STORAGE_BUFFER) {
    gCtx->shader_storage_buffer_accessor[index].assigned = true;
    gCtx->shader_storage_buffer_accessor[index].buffer_index = buffer;
    gCtx->shader_storage_buffer_accessor[index].offset =
        static_cast<size_t>(offset);
    gCtx->shader_storage_buffer_accessor[index].size =
        static_cast<size_t>(size);
  } else if (target == GL_UNIFORM_BUFFER) {
    gCtx->uniform_buffer_accessor[index].assigned = true;
    gCtx->uniform_buffer_accessor[index].buffer_index = buffer;
    gCtx->uniform_buffer_accessor[index].offset = static_cast<size_t>(offset);
    gCtx->uniform_buffer_accessor[index].size = static_cast<size_t>(size);
  }
}

void glBufferData(GLenum target, GLsizeiptr size, const GLvoid *data,
                  GLenum usage) {
  InitializeGLContext();
  assert((target == GL_SHADER_STORAGE_BUFFER) || (target == GL_UNIFORM_BUFFER));
  (void)target;

  if (gCtx->active_buffer_index == 0) return;

  gCtx->buffers[gCtx->active_buffer_index].data.resize(
      static_cast<size_t>(size));
  memcpy(gCtx->buffers[gCtx->active_buffer_index].data.data(), data,
         static_cast<size_t>(size));

  (void)usage;
}

void glUseProgram(GLuint program) {
  InitializeGLContext();
  gCtx->active_program = program;
}

void glShaderBinary(GLsizei n, const GLuint *shaders, GLenum binaryformat,
                    const void *binary, GLsizei length) {
  InitializeGLContext();

  // TODO(LTE): Support multiple shaders.
  assert(n == 1);
  assert(shaders);
  assert(binary);
  assert(length > 0);
  assert(length % 4 ==
         0);  // Size of SPIR-V binary must be the multiple of 32bit
  assert(binaryformat == GL_SHADER_BINARY_FORMAT_SPIR_V_ARB);
  (void)n;
  (void)binaryformat;

  size_t idx = shaders[0];
  if (gCtx->shaders[idx].deleted) {
    std::cerr << "[SoftGL] shader " << idx << " is not initialized."
              << std::endl;
    return;
  }

  gCtx->shaders[idx].binary.resize(static_cast<size_t>(length / 4));
  memcpy(gCtx->shaders[idx].binary.data(), binary, static_cast<size_t>(length));
}

void glShaderSource(GLuint shader, GLsizei count, const GLchar *const *string,
                    const GLint *length) {
  assert(count > 0);
  assert(string);

  size_t idx = shader;
  if (gCtx->shaders[idx].deleted) {
    std::cerr << "[SoftGL] shader " << idx << " is not initialized."
              << std::endl;
    return;
  }

  gCtx->shaders[idx].source.clear();

  // Concat
  for (size_t i = 0; i < static_cast<size_t>(count); i++) {
    if (length) {
      gCtx->shaders[idx].source +=
          std::string(string[i], static_cast<size_t>(length[i]));
    } else {
      // Assume each input string is null terminated.
      gCtx->shaders[idx].source += std::string(string[i]);
    }
  }
}

GLint glGetUniformLocation(GLuint program, const GLchar *name) {
  InitializeGLContext();
  (void)name;

  if (program == 0) {
    return -1;
  }

  const Program &prog = gCtx->programs[program];

  if (prog.deleted) {
    // LOG_F(ERROR, "Program %d is not created.", program);
    return -1;
  }

  // ABORT_F("TODO");
  return -1;
}

void glAttachShader(GLuint program, GLuint shader) {
  InitializeGLContext();

  if (program == 0) return;
  if (shader == 0) return;

  assert(program < gCtx->programs.size());

  gCtx->programs[program].shaders.push_back(shader);
}

void glGetShaderiv(GLuint shader, GLenum pname, GLint *params) {
  if (shader == 0) {
    return;
  }

  InitializeGLContext();

  if (pname == GL_COMPILE_STATUS) {
    const Shader &s = gCtx->shaders[shader];
    if (s.binary.size() > 0) {
      if (params) {
        (*params) = GL_TRUE;
      }
    } else {
      if (params) {
        (*params) = GL_FALSE;
      }
    }
  } else {
    // ABORT_F("TODO");
  }

  return;
}

void glGetProgramiv(GLuint program, GLenum pname, GLint *params) {
  if (program == 0) {
    return;
  }

  if (pname == GL_LINK_STATUS) {
    const Program &prog = gCtx->programs[program];

    if (prog.linked) {
      if (params) {
        (*params) = GL_TRUE;
      }
    } else {
      if (params) {
        (*params) = GL_FALSE;
      }
    }

  } else {
    // ABORT_F("TODO");
  }

  return;
}

void glGetShaderInfoLog(GLuint shader, GLsizei maxLength, GLsizei *length,
                        GLchar *infoLog) {
  (void)shader;
  (void)maxLength;
  (void)length;
  (void)infoLog;

  // LOG_F(ERROR, "Implement");

  return;
}

void glDeleteShader(GLuint shader) {
  InitializeGLContext();

  if (shader == 0) return;

  assert(shader < gCtx->shaders.size());

  gCtx->shaders[shader].deleted = true;

  // TODO(LTE): Free shader resource.
}

void glDeleteProgram(GLuint program) {
  InitializeGLContext();

  if (program == 0) return;

  assert(program < gCtx->programs.size());

  gCtx->programs[program].deleted = true;

  softcompute::ShaderInstance *instance = gCtx->programs[program].instance.get();
  assert(instance);

  spirv_cross_get_interface_fn interface_fun =
      reinterpret_cast<spirv_cross_get_interface_fn>(
          instance->GetInterfaceFuncPtr());

  assert(gCtx->programs[program].shader);
  interface_fun()->destruct(gCtx->programs[program].shader.get());

  gCtx->programs[program].deleted = true;
}

void glDispatchCompute(GLuint num_groups_x, GLuint num_groups_y,
                       GLuint num_groups_z) {
  InitializeGLContext();
  (void)num_groups_z;

  // CHECK_F(num_groups_z == 1, "num_group_z must be 1 for a while");

  glm::uvec3 num_workgroups(num_groups_x, num_groups_y, 1);
  glm::uvec3 work_group_id(0, 0, 0);

  if (gCtx->active_program == 0) return;

  spirv_cross_shader_t *shader = gCtx->programs[gCtx->active_program].shader.get();
  spirv_cross_get_interface_fn interface_fun =
      reinterpret_cast<spirv_cross_get_interface_fn>(
          gCtx->programs[gCtx->active_program].instance->GetInterfaceFuncPtr());

  spirv_cross_set_builtin(shader, SPIRV_CROSS_BUILTIN_NUM_WORK_GROUPS,
                          &num_workgroups, sizeof(num_workgroups));
  spirv_cross_set_builtin(shader, SPIRV_CROSS_BUILTIN_WORK_GROUP_ID,
                          &work_group_id, sizeof(work_group_id));

  {
    auto t_begin = std::chrono::high_resolution_clock::now();
    // Execute work groups
    for (uint32_t y = 0; y < num_groups_y; y++) {
      for (uint32_t x = 0; x < num_groups_x; x++) {
        work_group_id.x = x;
        work_group_id.y = y;

        interface_fun()->invoke(shader);
      }
    }
    auto t_end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double, std::milli> exec_ms = t_end - t_begin;
    std::cout << "execute time: " << exec_ms.count() << " ms" << std::endl;
  }
}

void glCompileShader(GLuint shader_id) {
  InitializeGLContext();

  if (shader_id == 0) return;

  // CHECK_F(shader_id < gCtx->shaders.size(), "Invalid shader ID. Must be less
  // than %d but got %d", int(gCtx->shaders.size()), shader_id);

  Shader &shader = gCtx->shaders[shader_id];

  if (shader.binary.size() > 0) {
    // Binary shader attached.
    // TODO(LTE): Need to emit GL error?
    return;
  }

  if (shader.source.size() == 0) {
    // No shader source input
    // TODO(LTE): Need to emit GL error?
    return;
  }

#if 0
  std::string basename = GenerateUniqueFilename();
  std::string spirv_filename = basename + ".spv";
  std::string cpp_filename = basename + ".cc";

#ifdef _WIN32
  std::string dll_filename = basename + ".dll";
#else
  std::string dll_filename = basename + ".so";
#endif

  std::string glsl_filename = basename + ".comp";

  {
    std::ofstream ofs(glsl_filename);
    if (!ofs) {
      std::cerr << "Failed to write shader source to [" << glsl_filename << "]"
                << std::endl;
      return;
    }

    ofs << shader.source;
    ofs.close();
  }

  bool ret = compile_glsl(spirv_filename, /* verbose */ true, glsl_filename);
  if (!ret) {
    // LOG_F(ERROR, "Failed to compile GLSL to SPIR-V");
    return;
  }

  {
    std::vector<uint32_t> buf;
    {
      std::ifstream ifs(spirv_filename);
      if (!ifs) {
        // LOG_F(ERROR, "Failed to read SPIR-V file : %s",
        // spirv_filename.c_str());
        return;
      }

      ifs.seekg(0, ifs.end);
      int length = static_cast<int>(ifs.tellg());
      // CHECK_F((length > 0) && (length % 4 == 0), "Size of SPIR-V binary must
      // be the multile of 32bit, but got %d", length);
      ifs.seekg(0, ifs.beg);

      buf.resize(static_cast<size_t>(length / 4));

      ifs.read(reinterpret_cast<char *>(buf.data()), length);
    }

    shader.binary = std::move(buf);
  }
#else
  //ShHandle compiler = ShConstructCompiler(/* lang */EShLangCompute, /* debugOpts */0);

  glslang::InitializeProcess(); // Required before calling glslang functions.

  bool ret = compile_glsl_string(shader.source, "dummy", &shader.binary);
  if (!ret) {
    shader.binary.clear(); // for sure
  }
  std::cout << "len = " << shader.binary.size() << std::endl;

  glslang::FinalizeProcess();

#endif
}

GLuint glGetProgramResourceIndex(GLuint program, GLenum programInterface,
                                 const char *name) {
  InitializeGLContext();

  if (program == 0) return 0;

  if (programInterface == GL_SHADER_STORAGE_BLOCK) {
    // OK
  } else {
    SetGLError(GL_INVALID_ENUM);
    return 0;
  }
  assert(program < gCtx->programs.size());

  const Program &prog = gCtx->programs[program];

  GLuint idx = 0;
  const spirv_cross::ShaderResources resources =
      prog.cpp->get_shader_resources();

  for (size_t i = 0; i < resources.storage_buffers.size(); i++) {
    if (resources.storage_buffers[i].name.compare(name) == 0) {
      // Got it
      idx = static_cast<GLuint>(i);
    }
  }

  return idx;
}

void glShaderStorageBlockBinding(GLuint program, GLuint shaderBlockIndex,
                                 GLuint storageBlockBinding) {
  (void)program;
  (void)shaderBlockIndex;
  (void)storageBlockBinding;
  // LOG_F(ERROR, "TODO");
}

GLuint glGetUniformBlockIndex(GLuint program, const GLchar *uniformBlockName) {
  InitializeGLContext();

  if (program == 0) {
    return GL_INVALID_INDEX;
  }

  Program &prog = gCtx->GetProgram(program);

  const spirv_cross::ShaderResources resources =
      prog.cpp->get_shader_resources();

  GLuint idx = GL_INVALID_INDEX;
  for (size_t i = 0; i < resources.storage_buffers.size(); i++) {
    if (resources.storage_buffers[i].name.compare(uniformBlockName) == 0) {
      // Got it
      idx = static_cast<GLuint>(i);
    }
  }

  return idx;
}

}  // namespace softgl

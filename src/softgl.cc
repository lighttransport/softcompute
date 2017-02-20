#include "softgl.h"

#include <cassert>
#include <vector>
#include <map>
#include <iostream>
#include <algorithm>
#include <fstream>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreserved-id-macro"
#pragma clang diagnostic ignored "-Wdocumentation"
#pragma clang diagnostic ignored "-Wundefined-reinterpret-cast"
#pragma clang diagnostic ignored "-Wold-style-cast"
#pragma clang diagnostic ignored "-Wsign-conversion"
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wshadow"
#pragma clang diagnostic ignored "-Wswitch-enum"
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wdouble-promotion"
#pragma clang diagnostic ignored "-Wcast-align"
#pragma clang diagnostic ignored "-Wimplicit-fallthrough"
#pragma clang diagnostic ignored "-Wmissing-prototypes"
#pragma clang diagnostic ignored "-Wdeprecated"
#pragma clang diagnostic ignored "-Wweak-vtables"
#endif

#include "spirv_cross/external_interface.h"
#include "spirv_cross/internal_interface.hpp"
#include "spirv_cpp.hpp"
#include "spirv_glsl.hpp"

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#ifdef SOFTCOMPUTE_ENABLE_JIT
#include "jit-engine.h"
#else
#include "dll-engine.h"
#endif

namespace softgl
{

typedef struct spirv_cross_interface *(*spirv_cross_get_interface_fn)();

const int kMaxUniforms = 64;
const int kMaxBuffers = 64;
const int kMaxPrograms = 64;
const int kMaxShaders = 64;

struct Buffer
{
    std::vector<uint8_t> data;
    bool deleted;
    char pad[7];

    Buffer()
    {
        deleted = true;
    }
};

struct Accessor
{
    size_t offset;
    size_t size;
    uint32_t buffer_index;
    bool assigned;
    char pad[7];
    int pad1;

    Accessor()
    {
        offset = 0;
        size = 0;
        buffer_index = 0;
        assigned = false;
    }
};

struct Uniform
{
    union {
        float fv[4];
        int iv[4];
    };

    int type;
    int count;
};

struct Program
{
  std::vector<uint32_t> shaders; // List of attached shaders

  bool deleted;
  bool linked;
  char buf[6];

  spirv_cross::CompilerGLSL* glsl;

  softcompute::ShaderInstance *instance;
  spirv_cross_shader_t *shader;

  Program() {
    deleted = true;
    linked = false;
    glsl = nullptr;
    instance = nullptr;
    shader = nullptr;
  }

};

struct Shader
{
  std::vector<uint32_t> binary; // Shader binary input
  std::string source; // Shader source input

  bool deleted;
  char buf[7];

  Shader() {
    deleted = true;
  }
};

class SoftGLContext
{
public:
    SoftGLContext()
    {
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

    ~SoftGLContext()
    {
    }

    uint32_t active_buffer_index;
    uint32_t active_program;

    std::vector<Accessor> shader_storage_buffer_accessor;
    std::vector<Accessor> uniform_buffer_accessor;
    std::vector<Uniform> uniforms;
    std::vector<Buffer> buffers;
    std::vector<Program> programs;
    std::vector<Shader> shaders;

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
      _mktemp_s(buf, len); // prefix will be overwritten with actual filename
  if (err != 0) {
    std::cerr << "Failed to create unique filename.\n";
    return std::string();
  }

  printf("DBG: Unique name: %s", buf);

  std::string name = std::string(buf);

#else
  int fd = mkstemp(basename); // prefix will be overwritten with actual filename
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

static bool exec_command(std::vector<std::string> *outputs, const std::string &cmd)
{
    outputs->clear();

    // This may not be needed, but for the safety.
    // See popen(3) manual for details why calling fflush(NULL) here
    fflush(NULL);

#if defined(_WIN32)
    FILE *pfp = _popen(cmd.c_str(), "r");
#else
    FILE *pfp = popen(cmd.c_str(), "r");
#endif

    if (!pfp)
    {
        std::cerr << "Failed to open pipe." << std::endl;
        perror("popen");

        return false;
    }

    char buf[4096];
    while (fgets(buf, 4095, pfp) != NULL)
    {
        //printf("%s", buf);
        outputs->push_back(buf);
    }

#if defined(_WIN32)
    int status = _pclose(pfp);
    if (status == -1)
    {
        fprintf(stderr, "Failed to close pipe.\n");
        return false;
    }
#else
    int status = pclose(pfp);
    if (status == -1)
    {
        fprintf(stderr, "Failed to close pipe.\n");
        return false;
    }
#endif

    return true;
}

// glsl -> spirv
static bool compile_glsl(const std::string &output_filename, bool verbose, const std::string &glsl_filename)
{

    //
    // Invoke `glslangValidator`
    //
    std::string binary = "glslangValidator";
    char *binary_path = getenv("GLSLANG_VALIDADOR");
    if (binary_path)
    {
        binary = binary_path;
        if (verbose)
        {
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
            std::cerr << "Failed to translate GLSL to SPIR-V" << std::endl;
            return false;
        }
    }

    return true;
}

// spirv -> c++
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
            std::cerr << "Failed to translate SPIR-V to C++" << std::endl;
            return false;
        }
      return true;
    }

    return false;

}

// c++ -> dll
static bool compile_cpp(const std::string &output_filename, const std::string &options, bool verbose,
                 const std::string &cpp_filename)
{

    //
    // Invoke C++ compiler
    //
    std::string cpp = "g++";
    char *cpp_env = getenv("CXX");
    if (cpp_env)
    {
        cpp = cpp_env;
        if (verbose)
        {
            printf("cpp = %s\n", cpp.c_str());
        }
    }

    // Assume gcc or clang. Assume mingw on windows.
    std::stringstream ss;
    ss << cpp;
    ss << " -std=c++11";
    ss << " -I./glm"; // TODO(syoyo): User-supplied path to glm
    ss << " -I./SPIRV-Cross/include"; // TODO(syoyo): User-supplied path to SPIRV-Cross/include.
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
    if (verbose)
        std::cout << cmd << std::endl;

    std::vector<std::string> outputs;
    bool ret = exec_command(&outputs, cmd);
    if (ret)
    {
        //
        // Check if compiled dll file exists.
        //
        std::ifstream ifile(output_filename);
        if (!ifile)
        {
            std::cerr << "Failed to compile C++" << std::endl;
            return false;
        }
    }

    return true;
}

// -------------------------------------------------------------------

void InitSoftGL()
{
    gCtx = new SoftGLContext();
}

void ReleaseSoftGL()
{
    delete gCtx;
    gCtx = nullptr;
}

static void InitializeGLContext()
{
    if (gCtx == nullptr)
    {
        InitSoftGL();
    }
}

void glUniform1f(GLint location, GLfloat v0)
{
    InitializeGLContext();
    if (location < 0) return;
    assert(static_cast<size_t>(location) < gCtx->uniforms.size());

    gCtx->uniforms[static_cast<size_t>(location)].fv[0] = v0;
    gCtx->uniforms[static_cast<size_t>(location)].type = GL_FLOAT;
    gCtx->uniforms[static_cast<size_t>(location)].count = 1;
}

void glUniform2f(GLint location, GLfloat v0, GLfloat v1)
{

    InitializeGLContext();
    if (location < 0) return;
    assert(static_cast<size_t>(location) < gCtx->uniforms.size());

    gCtx->uniforms[static_cast<size_t>(location)].fv[0] = v0;
    gCtx->uniforms[static_cast<size_t>(location)].fv[1] = v1;
    gCtx->uniforms[static_cast<size_t>(location)].type = GL_FLOAT;
    gCtx->uniforms[static_cast<size_t>(location)].count = 2;
}

void glUniform3f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2)
{
    InitializeGLContext();
    if (location < 0) return;
    assert(static_cast<size_t>(location) < gCtx->uniforms.size());

    gCtx->uniforms[static_cast<size_t>(location)].fv[0] = v0;
    gCtx->uniforms[static_cast<size_t>(location)].fv[1] = v1;
    gCtx->uniforms[static_cast<size_t>(location)].fv[2] = v2;
    gCtx->uniforms[static_cast<size_t>(location)].type = GL_FLOAT;
    gCtx->uniforms[static_cast<size_t>(location)].count = 3;
}

void glUniform4f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3)
{
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

void glUniform1i(GLint location, GLint v0)
{
    InitializeGLContext();
    if (location < 0) return;
    assert(static_cast<size_t>(location) < gCtx->uniforms.size());

    gCtx->uniforms[static_cast<size_t>(location)].iv[0] = v0;
    gCtx->uniforms[static_cast<size_t>(location)].type = GL_INT;
    gCtx->uniforms[static_cast<size_t>(location)].count = 1;
}

void glUniform2i(GLint location, GLint v0, GLint v1)
{
    InitializeGLContext();
    if (location < 0) return;
    assert(static_cast<size_t>(location) < gCtx->uniforms.size());

    gCtx->uniforms[static_cast<size_t>(location)].iv[0] = v0;
    gCtx->uniforms[static_cast<size_t>(location)].iv[1] = v1;
    gCtx->uniforms[static_cast<size_t>(location)].type = GL_INT;
    gCtx->uniforms[static_cast<size_t>(location)].count = 2;
}

void glUniform3i(GLint location, GLint v0, GLint v1, GLint v2)
{
    InitializeGLContext();
    if (location < 0) return;
    assert(static_cast<size_t>(location) < gCtx->uniforms.size());

    gCtx->uniforms[static_cast<size_t>(location)].iv[0] = v0;
    gCtx->uniforms[static_cast<size_t>(location)].iv[1] = v1;
    gCtx->uniforms[static_cast<size_t>(location)].iv[2] = v2;
    gCtx->uniforms[static_cast<size_t>(location)].type = GL_INT;
    gCtx->uniforms[static_cast<size_t>(location)].count = 3;
}

void glUniform4i(GLint location, GLint v0, GLint v1, GLint v2, GLint v3)
{
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

void glGenBuffers(GLsizei n, GLuint *buffers)
{
    InitializeGLContext();
    assert(n >= 0);
    assert(n < kMaxBuffers);
    assert(buffers);

    // Simple linear seach to find available Buffer.
    // Skip 0th index since its reserved.
    int count = 0;
    for (size_t i = 1; i < gCtx->buffers.size(); i++)
    {
        if (gCtx->buffers[i].deleted)
        {
            buffers[count] = static_cast<GLuint>(i);
            gCtx->buffers[i].deleted = false;
            count++;

            if (count >= n)
                break;
        }
    }
}

GLuint glCreateProgram()
{
    InitializeGLContext();

    // Simple linear seach to find available Program.
    // Skip 0th index since its reserved.
    for (size_t i = 1; i < gCtx->programs.size(); i++)
    {
        if (gCtx->programs[i].deleted)
        {
            gCtx->programs[i].deleted = false;
            return static_cast<GLuint>(i);
        }
    }

    return 0; // Invalid
}

GLuint glCreateShader(GLenum shader_type)
{
  assert(shader_type == GL_COMPUTE_SHADER);

  // Simple linear seach to find available Shader.
  // Skip 0th index since its reserved.
  for (size_t i = 1; i < gCtx->shaders.size(); i++)
  {
      if (gCtx->shaders[i].deleted)
      {
          gCtx->shaders[i].deleted = false;
          return static_cast<GLuint>(i);
      }
  }

  return 0;
}

void glLinkProgram( GLuint program)
{
  InitializeGLContext();
  
  assert(program < kMaxPrograms);

  if (program == 0) return;

  Program &prog = gCtx->programs[program];

  if (prog.deleted) {
    std::cerr << "[SoftGL] Program " << program << " is not created." << std::endl;
    return;
  }

  // Currently only one shader per program.
  assert(prog.shaders.size() == 1);

  GLuint shader_idx = prog.shaders[0];

  if (shader_idx == 0) return;

  assert(shader_idx < gCtx->shaders.size());

  const Shader &shader = gCtx->shaders[shader_idx];

  std::string basename = GenerateUniqueFilename();
  std::string spirv_filename = basename + ".spv";
  std::string cpp_filename = basename + ".cc";

#ifdef _WIN32
  std::string dll_filename = basename + ".dll";
#else
  std::string dll_filename = basename + ".so";
#endif

  if (shader.source.size() > 0) {
    // Assume GLSL source

    std::string glsl_filename = basename + ".comp";

    {
      std::ofstream ofs(glsl_filename);
      if (!ofs) {
        std::cerr << "Failed to write shader source to [" << glsl_filename << "]" << std::endl;
        return;
      }

      ofs << shader.source;
      ofs.close();
    }

    bool ret = compile_glsl(spirv_filename, /* verbose */true, glsl_filename);
    if (!ret) {
      std::cerr << "Failed to compile GLSL to SPIR-V" << std::endl;
      return;
    }

    ret = compile_spirv(cpp_filename, /* verbose */true, spirv_filename);
    if (!ret) {
      std::cerr << "Failed to compile SPIR-V to C++" << std::endl;
      return;
    }

    {
      std::vector<uint32_t> buf;
      {
        std::ifstream ifs(spirv_filename);
        if (!ifs) {
          std::cerr << "Failed to read SPIR-V file : " << spirv_filename << std::endl;
          return;
        }

        ifs.seekg (0, ifs.end);
        int length = static_cast<int>(ifs.tellg());
        assert((length > 0) && (length % 4 == 0));  // Size of SPIR-V binary must be the multile of 32bit
        ifs.seekg (0, ifs.beg);

        buf.resize(static_cast<size_t>(length / 4));

        ifs.read(reinterpret_cast<char*>(buf.data()), length);
      }

      // Initialize GLSL context of SPIRV-Cross
      prog.glsl = new spirv_cross::CompilerGLSL(std::move(buf));
      assert(prog.glsl);
    }


  } else if (shader.binary.size() > 0) {
    // Assume SPIR-V binary

    {
      std::ofstream ofs(spirv_filename);
      if (!ofs) {
        std::cerr << "Failed to write SPIR-V binary to [" << spirv_filename << "]" << std::endl;
        return;
      }

      ofs.write(reinterpret_cast<const char*>(shader.binary.data()), static_cast<ssize_t>(shader.binary.size() * sizeof(uint32_t)));
      ofs.close();
    }

    bool ret = compile_spirv(cpp_filename, /* verbose */true, spirv_filename);
    if (!ret) {
      std::cerr << "Failed to translate SPIR-V to C++" << std::endl;
      return;
    }

    // Initialize GLSL context of SPIRV-Cross
    prog.glsl = new spirv_cross::CompilerGLSL(shader.binary);
    assert(prog.glsl);

  } else {
    assert(0);
  }

  std::string compile_options; // FIXME(syoyo): Supply compiler options

  // c++ -> dll
  bool ret = compile_cpp(dll_filename, compile_options, /* verbose */true, cpp_filename);
  if (!ret) {
    std::cerr << "Failed to compile C++ into dll module." << std::endl;
    return;
  }

  prog.instance = nullptr; // FIXME(LTE)
  //softcompute::ShaderInstance *instance = nullptr;
  //      engine.Compile("comp", /* id */ 0, paths, compiler_options, source_filename);

  spirv_cross_get_interface_fn interface = reinterpret_cast<spirv_cross_get_interface_fn>(prog.instance->GetInterface());
  
  prog.shader = interface()->construct();

  prog.linked = true;
}


void glBindBuffer(GLenum target, GLuint buffer)
{
    InitializeGLContext();
    assert((target == GL_SHADER_STORAGE_BUFFER) || (target == GL_UNIFORM_BUFFER));
    (void)target;

    gCtx->active_buffer_index = buffer;
}

#if 0
void glBindBufferBase(GLenum target, GLuint index, GLuint buffer)
{
    InitializeGLContext();
    assert((target == GL_SHADER_STORAGE_BUFFER) || (target == GL_UNIFORM_BUFFER));

    assert(index <= kMaxBuffers);

    assert(buffer < gCtx->buffers.size());
    assert(gCtx->buffers[buffer].deleted == false);

    // TODO(LTE): Implement
    assert(0);
    if (target == GL_SHADER_STORAGE_BUFFER)
    {
    }
    else if (target == GL_UNIFORM_BUFFER)
    {
    }

  return;
}
#endif

void glBindBufferRange(GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size)
{
    InitializeGLContext();
    assert((target == GL_SHADER_STORAGE_BUFFER) || (target == GL_UNIFORM_BUFFER));

    assert(index <= kMaxBuffers);
    assert(buffer < gCtx->buffers.size());
    assert(gCtx->buffers[buffer].deleted == false);

    assert(static_cast<size_t>(offset + size) <= gCtx->buffers[buffer].data.size());

    if (target == GL_SHADER_STORAGE_BUFFER)
    {
        gCtx->shader_storage_buffer_accessor[index].assigned = true;
        gCtx->shader_storage_buffer_accessor[index].buffer_index = buffer;
        gCtx->shader_storage_buffer_accessor[index].offset = static_cast<size_t>(offset);
        gCtx->shader_storage_buffer_accessor[index].size = static_cast<size_t>(size);
    }
    else if (target == GL_UNIFORM_BUFFER)
    {
        gCtx->uniform_buffer_accessor[index].assigned = true;
        gCtx->uniform_buffer_accessor[index].buffer_index = buffer;
        gCtx->uniform_buffer_accessor[index].offset = static_cast<size_t>(offset);
        gCtx->uniform_buffer_accessor[index].size = static_cast<size_t>(size);
    }
}

void glBufferData(GLenum target, GLsizeiptr size, const GLvoid *data, GLenum usage)
{
    InitializeGLContext();
    assert((target == GL_SHADER_STORAGE_BUFFER) || (target == GL_UNIFORM_BUFFER));

    if (gCtx->active_buffer_index == 0)
        return;

    gCtx->buffers[gCtx->active_buffer_index].data.resize(static_cast<size_t>(size));
    memcpy(gCtx->buffers[gCtx->active_buffer_index].data.data(), data, static_cast<size_t>(size));

    (void)usage;
}

void glUseProgram(GLuint program)
{
  InitializeGLContext();
  gCtx->active_program = program;
}

void glShaderBinary(  GLsizei n,
  const GLuint *shaders,
  GLenum binaryformat,
  const void *binary,
  GLsizei length)
{
  InitializeGLContext();

  // TODO(LTE): Support multiple shaders.
  assert(n == 1);
  assert(shaders);
  assert(binary);
  assert(length > 0);
  assert(length % 4 == 0);  // Size of SPIR-V binary must be the multiple of 32bit
  assert(binaryformat == GL_SHADER_BINARY_FORMAT_SPIR_V_ARB);
  
  size_t idx = shaders[0];
  if (gCtx->shaders[idx].deleted) {
    std::cerr << "[SoftGL] shader " << idx << " is not initialized." << std::endl;
    return;
  }

  gCtx->shaders[idx].binary.resize(static_cast<size_t>(length/4));
  memcpy(gCtx->shaders[idx].binary.data(), binary, static_cast<size_t>(length));
}

void glShaderSource(  GLuint shader,
  GLsizei count,
  const GLchar * const *string,
  const GLint *length)
{
  assert(count > 0);
  assert(string);
  assert(length);

  size_t idx = shader;
  if (gCtx->shaders[idx].deleted) {
    std::cerr << "[SoftGL] shader " << idx << " is not initialized." << std::endl;
    return;
  }

  gCtx->shaders[idx].source.clear();
  
  // Concat
  for (size_t i = 0; i < static_cast<size_t>(count); i++) {
    gCtx->shaders[idx].source += std::string(string[i], static_cast<size_t>(length[i]));
  }

}

GLint glGetUniformLocation( GLuint program,
  const GLchar *name)
{
  InitializeGLContext();
  (void)program;
  //if (gCtx->glsl_program_map.find(program) == gCtx->glsl_program_map.end()) {
  //  return -1;
  //}

  (void)name;

  return -1;
}

void glAttachShader(GLuint program, GLuint shader)
{
  InitializeGLContext();

  if (program == 0) return;
  if (shader == 0) return;

  assert(program < gCtx->programs.size());

  gCtx->programs[program].shaders.push_back(shader);
}

void glDeleteProgram(GLuint program)
{
  InitializeGLContext();

  if (program == 0) return;

  assert(program < gCtx->programs.size());

  gCtx->programs[program].deleted = true;

  softcompute::ShaderInstance *instance = gCtx->programs[program].instance;
  assert(instance);

  spirv_cross_get_interface_fn interface = reinterpret_cast<spirv_cross_get_interface_fn>(instance->GetInterface());

  assert(gCtx->programs[program].shader);
  interface()->destruct(gCtx->programs[program].shader);
  
  gCtx->programs[program].deleted = true;
  
}

void glDispatchCompute(GLuint num_groups_x, GLuint num_groups_y, GLuint num_groups_z)
{
  assert(num_groups_z == 1); // TODO(LTE): Support 3D dispatch

  glm::uvec3 num_workgroups(num_groups_x, num_groups_y, 1);
  glm::uvec3 work_group_id(0, 0, 0);

  if (gCtx->active_program == 0) return;

  spirv_cross_shader_t *shader = gCtx->programs[gCtx->active_program].shader;
  spirv_cross_get_interface_fn interface = reinterpret_cast<spirv_cross_get_interface_fn>(gCtx->programs[gCtx->active_program].instance->GetInterface());

  spirv_cross_set_builtin(shader, SPIRV_CROSS_BUILTIN_NUM_WORK_GROUPS, &num_workgroups, sizeof(num_workgroups));
  spirv_cross_set_builtin(shader, SPIRV_CROSS_BUILTIN_WORK_GROUP_ID, &work_group_id, sizeof(work_group_id));

  {
      auto t_begin = std::chrono::high_resolution_clock::now();
      // Execute work groups
      for (uint32_t y = 0; y < num_groups_y; y++)
      {
          for (uint32_t x = 0; x < num_groups_x; x++)
          {
              work_group_id.x = x;
              work_group_id.y = y;

              interface()->invoke(shader);
          }
      }
      auto t_end = std::chrono::high_resolution_clock::now();

      std::chrono::duration<double, std::milli> exec_ms = t_end - t_begin;
      std::cout << "execute time: " << exec_ms.count() << " ms" << std::endl;
  }

}

} // namespace softgl

#ifndef SOFT_GL_H_
#define SOFT_GL_H_

#include <cstddef>
#include <cstdint>

// Simple software emulation of some OpenGL functions.

namespace softgl
{

const int GL_FALSE = 0;
const int GL_TRUE = 1;

typedef uint8_t GLboolean;
typedef uint32_t GLenum;
typedef int32_t GLint;
typedef float GLfloat;
typedef uint32_t GLuint;
typedef int32_t GLsizei;
typedef int8_t GLbyte;
typedef uint8_t GLubyte;
typedef float GLfloat;
typedef float GLclampf;
typedef double GLdouble;
typedef double GLclampd;
typedef void GLvoid;
typedef char GLchar;

typedef ptrdiff_t GLintptr;
typedef ptrdiff_t GLsizeiptr;

const int GL_COMPUTE_SHADER = 0x91B9;
const int GL_COMPILE_STATUS = 0x8B81;
const int GL_LINK_STATUS = 0x8B82;

const int GL_BYTE = 0x1400;
const int GL_UNSIGNED_BYTE = 0x1401;
const int GL_SHORT = 0x1402;
const int GL_UNSIGNED_SHORT = 0x1403;
const int GL_INT = 0x1404;
const int GL_UNSIGNED_INT = 0x1405;
const int GL_FLOAT = 0x1406;

const int GL_SHADER_BINARY_FORMAT_SPIR_V_ARB = 0x9551;

const int GL_SHADER_STORAGE_BARRIER_BIT = 0x2000;
const int GL_MAX_COMBINED_SHADER_OUTPUT_RESOURCES = 0x8F39;
const int GL_SHADER_STORAGE_BUFFER = 0x90D2;
const int GL_SHADER_STORAGE_BUFFER_BINDING = 0x90D3;
const int GL_SHADER_STORAGE_BUFFER_START = 0x90D4;
const int GL_SHADER_STORAGE_BUFFER_SIZE = 0x90D5;
const int GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS = 0x90D6;
const int GL_MAX_GEOMETRY_SHADER_STORAGE_BLOCKS = 0x90D7;
const int GL_MAX_TESS_CONTROL_SHADER_STORAGE_BLOCKS = 0x90D8;
const int GL_MAX_TESS_EVALUATION_SHADER_STORAGE_BLOCKS = 0x90D9;
const int GL_MAX_FRAGMENT_SHADER_STORAGE_BLOCKS = 0x90DA;
const int GL_MAX_COMPUTE_SHADER_STORAGE_BLOCKS = 0x90DB;
const int GL_MAX_COMBINED_SHADER_STORAGE_BLOCKS = 0x90DC;
const int GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS = 0x90DD;
const int GL_MAX_SHADER_STORAGE_BLOCK_SIZE = 0x90DE;
const int GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT = 0x90DF;

const int GL_UNIFORM_BUFFER = 0x8A11;

const int GL_SHADER_STORAGE_BLOCK = 0x92E6;

const int GL_NO_ERROR = 0;
const int GL_INVALID_ENUM = 0x0500;
const int GL_INVALID_VALUE = 0x0501;
const int GL_INVALID_OPERATION = 0x0502;
const int GL_STACK_OVERFLOW = 0x0503;
const int GL_STACK_UNDERFLOW = 0x0504;
const int GL_OUT_OF_MEMORY = 0x0505;

void glUniform1f(GLint location, GLfloat v0);

void glUniform2f(GLint location, GLfloat v0, GLfloat v1);

void glUniform3f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);

void glUniform4f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);

void glUniform1i(GLint location, GLint v0);

void glUniform2i(GLint location, GLint v0, GLint v1);

void glUniform3i(GLint location, GLint v0, GLint v1, GLint v2);

void glUniform4i(GLint location, GLint v0, GLint v1, GLint v2, GLint v3);

GLint glGetUniformLocation(GLuint program, const GLchar *name);

void glDispatchCompute(GLuint num_groups_x, GLuint num_groups_y, GLuint num_groups_z);

void glDispatchComputeIndirect(GLintptr indirect);

void glGenBuffers(GLsizei n, GLuint *buffers);

void glBindBuffer(GLenum target, GLuint buffer);
void glBindBufferBase(GLenum target, GLuint index, GLuint buffer);
void glBindBufferRange(GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size);

void glBufferData(GLenum target, GLsizeiptr size, const GLvoid *data, GLenum usage);

void *glMapBuffer(GLenum target, GLenum access);
GLboolean glUnmapBuffer(GLenum target);

GLuint glCreateProgram();
GLuint glCreateShader(GLenum shaderType);
void glUseProgram(GLuint program);
void glAttachShader(GLuint program, GLuint shader);
void glCompileShader( GLuint shader);
void glShaderSource(  GLuint shader,
  GLsizei count,
  const GLchar * const *string,
  const GLint *length);
void glShaderBinary(  GLsizei n,
  const GLuint *shaders,
  GLenum binaryformat,
  const void *binary,
  GLsizei length);
void glDeleteShader(GLuint shader);
void glGetShaderInfoLog(GLuint shader, GLsizei maxLength, GLsizei *length, GLchar *infoLog);
void glGetShaderiv(GLuint shader, GLenum pname, GLint *params);

GLuint glGetProgramResourceIndex(GLuint program, GLenum programInterface, const char *name);
void glShaderStorageBlockBinding(GLuint program, GLuint shaderBlockIndex, GLuint shaderBlockBinding);

void glLinkProgram(GLuint program);
void glDeleteProgram(GLuint program);
void glGetProgramiv(GLuint program, GLenum pname, GLint *params);
void glGetPrograminfoLog(GLuint program, GLsizei maxLength, GLsizei *length, GLchar *infoLog);

void InitSoftGL();
void ReleaseSoftGL();

} // softgl

#endif // SOFT_GL_H_

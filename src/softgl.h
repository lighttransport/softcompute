#ifndef SOFT_GL_H_
#define SOFT_GL_H_

#include <cstdint>
#include <cstddef>

// Simple software emulation of some OpenGL functions.

namespace softgl {

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

void glUniform1f( GLint location,
  GLfloat v0);
 
void glUniform2f( GLint location,
  GLfloat v0,
  GLfloat v1);
 
void glUniform3f( GLint location,
  GLfloat v0,
  GLfloat v1,
  GLfloat v2);
 
void glUniform4f( GLint location,
  GLfloat v0,
  GLfloat v1,
  GLfloat v2,
  GLfloat v3);
 
void glUniform1i( GLint location,
  GLint v0);
 
void glUniform2i( GLint location,
  GLint v0,
  GLint v1);
 
void glUniform3i( GLint location,
  GLint v0,
  GLint v1,
  GLint v2);
 
void glUniform4i( GLint location,
  GLint v0,
  GLint v1,
  GLint v2,
  GLint v3);

GLint glGetUniformLocation( GLuint program,
  const GLchar *name);

void glDispatchCompute( GLuint num_groups_x,
  GLuint num_groups_y,
  GLuint num_groups_z);

void glDispatchComputeIndirect( GLintptr indirect);

void glGenBuffers(  GLsizei n,
  GLuint * buffers);

void glBindBuffer(GLenum target, GLuint buffer);
void glBindBufferBase(GLenum target, GLuint index, GLuint buffer);
void glBindBufferRange(GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size);

void * glMapBuffer(GLenum target, GLenum access);
GLboolean glUnmapBuffer(GLenum target);


void InitSoftGL();
void ReleaseSoftGL();


}  // softgl

#endif // SOFT_GL_H_

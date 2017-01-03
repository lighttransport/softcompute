#ifndef SOFT_GL_H_
#define SOFT_GL_H_

// Simple software emulation of some OpenGL functions.

namespace softgl {

typedef int GLint;
typedef float GLfloat;

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

}  // softgl

#endif // SOFT_GL_H_

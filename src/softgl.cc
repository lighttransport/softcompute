#include "softgl.h"

#include <cassert>
#include <vector>

namespace softgl
{

const int kMaxUniforms = 64;
const int kMaxBuffers = 64;

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

class SoftGLContext
{
public:
    SoftGLContext()
    {
        // 0th index is reserved.
        uniforms.resize(kMaxUniforms + 1);
        buffers.resize(kMaxBuffers + 1);
        shader_storage_buffer_accessor.resize(kMaxBuffers + 1);
        uniform_buffer_accessor.resize(kMaxBuffers + 1);
    }

    ~SoftGLContext()
    {
    }

    uint32_t active_buffer_index;
    int pad0;

    std::vector<Accessor> shader_storage_buffer_accessor;
    std::vector<Accessor> uniform_buffer_accessor;
    std::vector<Uniform> uniforms;
    std::vector<Buffer> buffers;
};

static SoftGLContext *gCtx = nullptr;

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
    assert(location >= 0);
    assert(location < kMaxUniforms);

    gCtx->uniforms[static_cast<size_t>(location)].fv[0] = v0;
    gCtx->uniforms[static_cast<size_t>(location)].type = GL_FLOAT;
    gCtx->uniforms[static_cast<size_t>(location)].count = 1;
}

void glUniform2f(GLint location, GLfloat v0, GLfloat v1)
{

    InitializeGLContext();
    assert(location >= 0);
    assert(location < kMaxUniforms);

    gCtx->uniforms[static_cast<size_t>(location)].fv[0] = v0;
    gCtx->uniforms[static_cast<size_t>(location)].fv[1] = v1;
    gCtx->uniforms[static_cast<size_t>(location)].type = GL_FLOAT;
    gCtx->uniforms[static_cast<size_t>(location)].count = 2;
}

void glUniform3f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2)
{
    InitializeGLContext();
    assert(location >= 0);
    assert(location < kMaxUniforms);

    gCtx->uniforms[static_cast<size_t>(location)].fv[0] = v0;
    gCtx->uniforms[static_cast<size_t>(location)].fv[1] = v1;
    gCtx->uniforms[static_cast<size_t>(location)].fv[2] = v2;
    gCtx->uniforms[static_cast<size_t>(location)].type = GL_FLOAT;
    gCtx->uniforms[static_cast<size_t>(location)].count = 3;
}

void glUniform4f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3)
{
    InitializeGLContext();
    assert(location >= 0);
    assert(location < kMaxUniforms);

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
    assert(location >= 0);
    assert(location < kMaxUniforms);

    gCtx->uniforms[static_cast<size_t>(location)].iv[0] = v0;
    gCtx->uniforms[static_cast<size_t>(location)].type = GL_INT;
    gCtx->uniforms[static_cast<size_t>(location)].count = 1;
}

void glUniform2i(GLint location, GLint v0, GLint v1)
{
    InitializeGLContext();
    assert(location >= 0);
    assert(location < kMaxUniforms);

    gCtx->uniforms[static_cast<size_t>(location)].iv[0] = v0;
    gCtx->uniforms[static_cast<size_t>(location)].iv[1] = v1;
    gCtx->uniforms[static_cast<size_t>(location)].type = GL_INT;
    gCtx->uniforms[static_cast<size_t>(location)].count = 2;
}

void glUniform3i(GLint location, GLint v0, GLint v1, GLint v2)
{
    InitializeGLContext();
    assert(location >= 0);
    assert(location < kMaxUniforms);

    gCtx->uniforms[static_cast<size_t>(location)].iv[0] = v0;
    gCtx->uniforms[static_cast<size_t>(location)].iv[1] = v1;
    gCtx->uniforms[static_cast<size_t>(location)].iv[2] = v2;
    gCtx->uniforms[static_cast<size_t>(location)].type = GL_INT;
    gCtx->uniforms[static_cast<size_t>(location)].count = 3;
}

void glUniform4i(GLint location, GLint v0, GLint v1, GLint v2, GLint v3)
{
    InitializeGLContext();
    assert(location >= 0);
    assert(location < kMaxUniforms);

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

void glBindBuffer(GLenum target, GLuint buffer)
{
    InitializeGLContext();
    assert((target == GL_SHADER_STORAGE_BUFFER) || (target == GL_UNIFORM_BUFFER));
    (void)target;

    gCtx->active_buffer_index = buffer;
}

void glBindBufferBase(GLenum target, GLuint index, GLuint buffer)
{
    InitializeGLContext();
    assert((target == GL_SHADER_STORAGE_BUFFER) || (target == GL_UNIFORM_BUFFER));

    assert(index < kMaxBuffers);

    assert(buffer < kMaxBuffers);
    assert(gCtx->buffers[buffer].deleted == false);

    if (target == GL_SHADER_STORAGE_BUFFER)
    {
    }
    else if (target == GL_UNIFORM_BUFFER)
    {
    }
}

void glBindBufferRange(GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size)
{
    InitializeGLContext();
    assert((target == GL_SHADER_STORAGE_BUFFER) || (target == GL_UNIFORM_BUFFER));

    assert(index < kMaxBuffers);
    assert(buffer < kMaxBuffers);
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

} // namespace softgl

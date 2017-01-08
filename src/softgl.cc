#include "softgl.h"

namespace softgl {

class SoftGLContext {
 public:
  SoftGLContext() {}
  ~SoftGLContext() {}

};

static SoftGLContext* gCtx;

void InitSoftGL()
{
  gCtx = new SoftGLContext();
}

void ReleaseSoftGL()
{
  delete gCtx;
  gCtx = nullptr;
}

}  // namespace softgl

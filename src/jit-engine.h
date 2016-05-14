#ifndef JIT_ENGINE_H_
#define JIT_ENGINE_H_

#include <string>
#include <vector>
#include <map>

//#include "shader.h"
//#include "material.h"

namespace softcompute {

#ifdef _MSV_VER
#pragma pack(1)
#endif
typedef struct PACKED {
    float u, v;
    float s, t;
    int x, y, z, w;

} ShaderEnv;

typedef enum {
  SHADER_PARAM_TYPE_INVALID = 0,
  SHADER_PARAM_TYPE_FLOAT,
  SHADER_PARAM_TYPE_STRING,
  SHADER_PARAM_TYPE_VEC4
} ShaderParamType;

struct ShaderParam
{
  ShaderParamType     type;
  float               fValue;
  std::string         sValue;
  std::vector<float>  vfValue;

  ShaderParam() : type(SHADER_PARAM_TYPE_INVALID) {
  }
}; 

typedef std::map<std::string, ShaderParam> ShaderParamMap;

class ShaderInstance {
public:
  ShaderInstance();
  ~ShaderInstance();

  // type == "shader"
  bool Compile(const std::string& type, const std::vector<std::string>& paths, const std::string& filename, const ShaderParamMap& paramMap);

  /// Eval JIT compiled shader
  bool Eval(ShaderEnv *env);

  /// Eval JIT compiled imager
  bool EvalImager(ShaderEnv *env);

private:
  class Impl;
  Impl *impl;
};


class ShaderEngine {
public:
  ShaderEngine(bool abortOnFailure = false);
  ~ShaderEngine();

  /// Compile C shader.
  /// type = "shader" or "imager"
  ShaderInstance* Compile(const std::string& type, unsigned int shaderID, const std::vector<std::string>& paths, const std::string &filename, const ShaderParamMap& paramMap);

  ShaderInstance* GetShaderInstance(unsigned int shaderID) {
    // @todo { use array for faster lookup. }
    if (shaderInstanceMap_.find(shaderID) != shaderInstanceMap_.end()) {
      return shaderInstanceMap_[shaderID];
    } else {
      return NULL;
    }
  }

private:
  class Impl;
  Impl *impl;

  std::map<unsigned int, ShaderInstance*> shaderInstanceMap_;

};

}

#endif

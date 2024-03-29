// Copyright 2016-2018 Light Transport Entertainment, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef JIT_ENGINE_H_
#define JIT_ENGINE_H_

#include <map>
#include <string>
#include <vector>

namespace softcompute {

class ShaderInstance {
 public:
  ShaderInstance();
  ShaderInstance(const ShaderInstance &rhs);
  ~ShaderInstance();

  // type must be "comp" at this time.
  bool Compile(const std::string &type, const std::vector<std::string> &paths,
               const std::string &options, const std::string &filename);

  void *GetInterfaceFuncPtr() const;

 private:
  class Impl;
  Impl *impl;
};

class ShaderEngine {
 public:
  ShaderEngine(bool abortOnFailure = false);
  ~ShaderEngine();

  /// Compile SPIRV-Cross generated cpp shader.
  ShaderInstance *Compile(const std::string &type, unsigned int shaderID,
                          const std::vector<std::string> &paths,
                          const std::string &options,
                          const std::string &filename);

  ShaderInstance *GetShaderInterface(uint32_t shaderID);

  ShaderInstance *GetShaderInstance(unsigned int shaderID) {
    // @todo { use array for faster lookup. }
    if (shaderInstanceMap_.find(shaderID) != shaderInstanceMap_.end()) {
      return shaderInstanceMap_[shaderID];
    } else {
      return nullptr;
    }
  }

 private:
  class Impl;
  Impl *impl;

  std::map<unsigned int, ShaderInstance *> shaderInstanceMap_;
};
}  // namespace softcompute

#endif

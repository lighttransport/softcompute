#ifndef JIT_ENGINE_H_
#define JIT_ENGINE_H_

#include <map>
#include <string>
#include <vector>

//#include "shader.h"
//#include "material.h"

namespace softcompute
{

class ShaderInstance
{
public:
	ShaderInstance();
	~ShaderInstance();

	// type must be "comp" at this time.
	bool Compile(const std::string &type, const std::vector<std::string> &paths, const std::string &filename);

	void *GetInterface();

private:
	class Impl;
	Impl *impl;
};

class ShaderEngine
{
public:
	ShaderEngine(bool abortOnFailure = false);
	~ShaderEngine();

	/// Compile SPIRV-Cross generated cpp shader.
	ShaderInstance *Compile(const std::string &type, unsigned int shaderID, const std::vector<std::string> &paths,
	                        const std::string &filename);

	ShaderInstance *GetShaderInstance(unsigned int shaderID)
	{
		// @todo { use array for faster lookup. }
		if (shaderInstanceMap_.find(shaderID) != shaderInstanceMap_.end())
		{
			return shaderInstanceMap_[shaderID];
		}
		else
		{
			return NULL;
		}
	}

private:
	class Impl;
	Impl *impl;

	std::map<unsigned int, ShaderInstance *> shaderInstanceMap_;
};
}

#endif

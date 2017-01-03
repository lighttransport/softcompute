// Copyright 2016 Light Transport Entertainment, Inc.
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

#include <cassert>
#include <cstdio>
#include <iostream>
#include <map>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "dll-engine.h"

static std::string GetFileExtension(const std::string &FileName)
{
    if (FileName.find_last_of(".") != std::string::npos)
        return FileName.substr(FileName.find_last_of(".") + 1);
    return "";
}

#ifdef _WIN32
std::wstring s2ws(const std::string &s)
{
    int len;
    int slength = (int)s.length() + 1;
    len = MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, 0, 0);
    wchar_t *buf = new wchar_t[len];
    MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, buf, len);
    std::wstring r(buf);
    delete[] buf;
    return r;
}
#endif

namespace softcompute
{

class ShaderInstance::Impl
{
public:
    Impl();
    ~Impl();

    bool Compile(const std::string &type, const std::vector<std::string> &paths, const std::string &filename);
    void *GetInterface();

private:
    void *entry_point_;
    void *handle_;
    std::string filename_;
};

ShaderInstance::Impl::Impl()
    : entry_point_(nullptr)
    , handle_(nullptr)
{
}

ShaderInstance::Impl::~Impl()
{
#if defined(_WIN32)
    if (handle_)
    {
        BOOL ret = FreeLibrary(reinterpret_cast<HMODULE>(handle_));
        assert(ret == TRUE);
        //return (ret == TRUE) ? true : false;
    }

    if (!filename_.empty())
    {
        int ret = unlink(filename_.c_str());
        if (ret == -1)
        {
            fprintf(stderr, "[DLLEngine] Failed to delete file: %s\n", filename_.c_str());
        }

        filename_ = std::string("");
    }

#else
    if (handle_)
    {
        int ret = dlclose(handle_);
        assert(ret == 0);
        //return (ret == 0) ? true : false;
    }

    if (!filename_.empty())
    {
        int ret = unlink(filename_.c_str());
        if (ret == -1)
        {
            fprintf(stderr, "[DLLEngine] Failed to delete file: %s\n", filename_.c_str());
        }

        filename_ = std::string("");
    }
#endif
}

bool ShaderInstance::Impl::Compile(const std::string &type, const std::vector<std::string> &paths,
                                   const std::string &filename)
{
    (void)type;
    (void)paths;

    std::string ext = GetFileExtension(filename);

    std::string abspath = filename; // @fixme
    if (abspath.empty())
    {
        fprintf(stderr, "[ShaderEngine] File not found in the search path: %s.\n", filename.c_str());
        return false;
    }

    void *handle = nullptr;

#ifdef _WIN32
    std::wstring s = s2ws(filename);
    HMODULE module = LoadLibrary(TEXT("./tmp.dll"));
    if (module == nullptr)
    {
        fprintf(stderr, "[DLLEngine] Cannot find/open shader file: %s\n", s.c_str());
        fprintf(stderr, "[DLLEngine] Err = %d\n", GetLastError());
        return false;
    }

    // Find shader body function
    entry_point_ = reinterpret_cast<void *>(GetProcAddress(module, "spirv_cross_get_interface"));
    if (entry_point_ == nullptr)
    {
        fprintf(stderr, "[DLLEngine] Cannot find `spirv_cross_get_interface` in : %s\n", s.c_str());
        FreeLibrary(module);
        return false;
    }

    handle_ = reinterpret_cast<void *>(handle);
    filename_ = filename;

#else
    std::string filepath = filename;
    handle = dlopen(filepath.c_str(), RTLD_NOW);

    if (handle != nullptr)
    {
        // Will be safe to delete .so file after dlopen().
        unlink(filename.c_str());
    }
    else
    {
        if ((filename.size() > 1) && ((filename[0] != '/') || (filename[0] != '.')))
        {
            // try to load from current path(this might have security risk?).
            filepath = std::string("./") + filename;
            handle = dlopen(filepath.c_str(), RTLD_NOW);
            printf("handle = %p\n", handle);

            // Will be safe to delete .so file after dlopen().
            unlink(filename.c_str());

            if (handle == nullptr)
            {
                fprintf(stderr, "[DLLEngine] Cannot find/open shader file: %s(err %s)\n", filepath.c_str(), dlerror());
            }
        }
        else
        {
            fprintf(stderr, "[DLLEngine] Cannot find/open shader file: %s(err %s)\n", filename.c_str(), dlerror());
            return false;
        }
    }

    // Find entry point
    entry_point_ = dlsym(handle, "spirv_cross_get_interface");
    if (entry_point_ == nullptr)
    {
        fprintf(stderr, "[DLLEngine] Cannot find `spirv_cross_get_interface` function in the dll: %s\n",
                filename.c_str());
        dlclose(handle);
        return false;
    }

    // Store handle & filename for later use.
    handle_ = handle;
    filename_ = filepath;
#endif

    printf("[DLLEngine] Shader [ %s ] compile OK.\n", filename.c_str());

    return true;
}

void *ShaderInstance::Impl::GetInterface()
{
    assert(entry_point_);
    return entry_point_;
}

ShaderInstance::ShaderInstance()
    : impl(new Impl())
{
}

ShaderInstance::~ShaderInstance()
{
    delete impl;
}

bool ShaderInstance::Compile(const std::string &type, const std::vector<std::string> &paths,
                             const std::string &filename)
{
    assert(impl);
    if (type != "comp")
    {
        std::cerr << "Unknown type: " << type << std::endl;
        return false;
    }

    return impl->Compile(type, paths, filename);
}

void *ShaderInstance::GetInterface()
{
    assert(impl);
    return impl->GetInterface();
}

//
// --
//

class ShaderEngine::Impl
{
public:
    Impl(bool abortOnFailure);
    ~Impl();

    ShaderInstance *Compile(const std::string &type, unsigned int shaderID, const std::vector<std::string> &paths,
                            const std::string &filename);

    void *GetInterface();

private:
    bool abortOnFailure_;
};

ShaderInstance *ShaderEngine::Impl::Compile(const std::string &type, unsigned int shaderID,
                                            const std::vector<std::string> &paths, const std::string &filename)
{
    (void)shaderID;

    ShaderInstance *shaderInstance = new ShaderInstance();
    bool ret = shaderInstance->Compile(type, paths, filename);
    if (!ret)
    {
        fprintf(stderr, "[Shader] Failed to compile shader: %s\n", filename.c_str());
        delete shaderInstance;
        return nullptr;
    }

    return shaderInstance;
}

ShaderEngine::Impl::Impl(bool abortOnFailure)
    : abortOnFailure_(abortOnFailure)
{
}

ShaderEngine::Impl::~Impl()
{
}

ShaderEngine::ShaderEngine(bool abortOnFailure)
    : impl(new Impl(abortOnFailure))
{
}

ShaderEngine::~ShaderEngine()
{
    delete impl;
}

ShaderInstance *ShaderEngine::Compile(const std::string &type, unsigned int shaderID,
                                      const std::vector<std::string> &paths, const std::string &options,
                                      const std::string &filename)
{

    (void)options;

    assert(impl);
    assert(shaderID != static_cast<unsigned int>(-1));

    if (shaderInstanceMap_.find(shaderID) != shaderInstanceMap_.end())
    {
        printf("[ShaderEngine] Err: Duplicate shader ID.\n");
        ShaderInstance *instance = shaderInstanceMap_[shaderID];
        if (instance)
        {
            delete instance;
        }
    }

    ShaderInstance *shaderInstance = impl->Compile(type, shaderID, paths, filename);

    shaderInstanceMap_[shaderID] = shaderInstance;

    return shaderInstance;
}

} // namespace softcompute

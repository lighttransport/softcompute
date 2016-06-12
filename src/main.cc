#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

#include <chrono>

#include <stdint.h>

#include "spirv_cross/external_interface.h"
#include "spirv_cross/internal_interface.hpp"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

extern spirv_cross_shader_t *spirv_cross_construct(void);
extern void spirv_cross_destruct(spirv_cross_shader_t *shader);
extern void spirv_cross_invoke(spirv_cross_shader_t *shader);

#ifdef ENABLE_JIT
#include "jit-engine.h"
#else
#include "dll-engine.h"
#endif

#include "OptionParser.h"

typedef struct spirv_cross_interface *(*spirv_cross_get_interface_fn)();

#define LOCAL_SIZE_X 16
#define LOCAL_SIZE_Y 16

#define WINDOW_SIZE 512

inline unsigned char fclamp(float x)
{
    int i = (int)(powf(x, 1.0 / 2.2) * 256.0f); // simple gamma correction
    if (i > 255)
        i = 255;
    if (i < 0)
        i = 0;

    return (unsigned char)i;
}

void SaveImageAsPNG(const char *filename, const float *rgba, int width, int height)
{

    std::vector<unsigned char> ldr(width * height * 3);
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            // Flip Y
            ldr[3 * ((height - y - 1) * width + x) + 0] = fclamp(rgba[4 * (y * width + x) + 0]);
            ldr[3 * ((height - y - 1) * width + x) + 1] = fclamp(rgba[4 * (y * width + x) + 1]);
            ldr[3 * ((height - y - 1) * width + x) + 2] = fclamp(rgba[4 * (y * width + x) + 2]);
        }
    }

    int len = stbi_write_png(filename, width, height, 3, &ldr.at(0), width * 3);
    if (len < 1)
    {
        printf("Failed to save image\n");
        exit(-1);
    }
}

bool exec_command(const std::string &cmd)
{
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
        printf("%s", buf);
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
bool compile_glsl(const std::string &output_filename, bool verbose, const std::string &glsl_filename)
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

    bool ret = exec_command(cmd);
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
bool compile_spirv(const std::string &output_filename, bool verbose, const std::string &spirv_filename)
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

    std::string cmd;
    cmd = ss.str();
    if (verbose)
    {
        std::cout << cmd << std::endl;
    }

    bool ret = exec_command(cmd);
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
    }

    return true;
}

// c++ -> dll
bool compile_cpp(const std::string &output_filename, const std::string &options, bool verbose,
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
    ss << " -I."; // @todo { set path to glm }
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

    bool ret = exec_command(cmd);
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

int main(int argc, char **argv)
{

    using optparse::OptionParser;

    OptionParser parser = OptionParser().description("options");

    parser.add_option("-o", "--options").help("Compiler options. e.g. \"-O2\"");
    parser.add_option("-v", "--verbose").action("store_true").set_default("false").help("Verbose mode.");

    optparse::Values options = parser.parse_args(argc, argv);
    std::vector<std::string> args = parser.args();

    if (args.empty())
    {
        parser.print_help();
        return -1;
    }

    bool verbose = false;
    if (options.get("verbose"))
    {
        verbose = true;
    }

    std::string compiler_options = options["options"];

    std::string filename = args[0];

    const char *ext = strrchr(filename.c_str(), '.');

    if (strcmp(ext, ".cc") == 0 || strcmp(ext, ".cpp") == 0)
    {
        // Assume C++
    }
    else if (strcmp(ext, ".spv") == 0)
    {
        // Assume SPIR-V binary
        std::string tmp_cc_filename = "tmp.cc";
        bool ret = compile_spirv(tmp_cc_filename, verbose, filename);
        if (!ret)
        {
            return -1;
        }

        filename = tmp_cc_filename;
    }
    else
    {
        // Assume GLSL
        std::string tmp_spv_filename = "tmp.spv";
        std::string tmp_cc_filename = "tmp.cc";
        bool ret = compile_glsl(tmp_spv_filename, verbose, filename);
        if (!ret)
        {
            return -1;
        }
        ret = compile_spirv(tmp_cc_filename, verbose, tmp_spv_filename);
        if (!ret)
        {
            return -1;
        }

        filename = tmp_cc_filename;
    }

    std::string source_filename;
#ifdef ENABLE_JIT
    source_filename = filename;
#else
#ifdef _WIN32
    std::string dll_filename = "tmp.dll"; // fixme.
#else
    std::string dll_filename = "tmp.so"; // fixme.
#endif
    bool ret = compile_cpp(dll_filename, compiler_options, verbose, filename);
    if (!ret)
    {
        return -1;
    }
    source_filename = dll_filename;
#endif

    softcompute::ShaderEngine engine;

    auto start_time = std::chrono::high_resolution_clock::now();

    std::vector<std::string> paths;
    softcompute::ShaderInstance *instance =
        engine.Compile("comp", /* id */ 0, paths, compiler_options, source_filename);

    if (!instance)
    {
        return EXIT_FAILURE;
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> ms = end_time - start_time;
    std::cout << "compile time: " << ms.count() << " ms" << std::endl;

    spirv_cross_get_interface_fn interface = reinterpret_cast<spirv_cross_get_interface_fn>(instance->GetInterface());
    spirv_cross_shader_t *shader = interface()->construct();

    // @fixme { parameter bindings are hardcoded for ao.cc }

    std::vector<float> outbuf(WINDOW_SIZE * WINDOW_SIZE * 4); // float4
    glm::uvec3 num_workgroups(WINDOW_SIZE / LOCAL_SIZE_X, WINDOW_SIZE / LOCAL_SIZE_Y, 1);
    glm::uvec3 work_group_id(0, 0, 0);

    void *outbuf_ptr = reinterpret_cast<void *>(outbuf.data());
    spirv_cross_set_resource(shader, 0, 0, &outbuf_ptr, sizeof(void *));

    spirv_cross_set_builtin(shader, SPIRV_CROSS_BUILTIN_NUM_WORK_GROUPS, &num_workgroups, sizeof(num_workgroups));
    spirv_cross_set_builtin(shader, SPIRV_CROSS_BUILTIN_WORK_GROUP_ID, &work_group_id, sizeof(work_group_id));

    {
        auto t_begin = std::chrono::high_resolution_clock::now();
        // Execute work groups
        for (int y = 0; y < WINDOW_SIZE / LOCAL_SIZE_Y; y++)
        {
            for (int x = 0; x < WINDOW_SIZE / LOCAL_SIZE_X; x++)
            {
                work_group_id.x = x;
                work_group_id.y = y;

                interface()->invoke(shader);
            }
        }
        auto t_end = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double, std::milli> ms = t_end - t_begin;
        std::cout << "execute time: " << ms.count() << " ms" << std::endl;
    }

    interface()->destruct(shader);

    // Save imag
    SaveImageAsPNG("output.png", &outbuf.at(0), WINDOW_SIZE, WINDOW_SIZE);

    std::cout << "output.png written." << std::endl;

    return EXIT_SUCCESS;
}

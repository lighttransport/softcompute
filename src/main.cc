#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

#include <chrono>

#include <stdint.h>

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreserved-id-macro"
#pragma clang diagnostic ignored "-Wdocumentation"
#pragma clang diagnostic ignored "-Wundefined-reinterpret-cast"
#pragma clang diagnostic ignored "-Wold-style-cast"
#pragma clang diagnostic ignored "-Wsign-conversion"
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wshadow"
#pragma clang diagnostic ignored "-Wswitch-enum"
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wdouble-promotion"
#pragma clang diagnostic ignored "-Wcast-align"
#pragma clang diagnostic ignored "-Wimplicit-fallthrough"
#pragma clang diagnostic ignored "-Wmissing-prototypes"
#pragma clang diagnostic ignored "-Wdeprecated"
#pragma clang diagnostic ignored "-Wweak-vtables"
#endif

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "OptionParser.h"

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include "softgl.h"

#if 0

extern spirv_cross_shader_t *spirv_cross_construct(void);
extern void spirv_cross_destruct(spirv_cross_shader_t *shader);
extern void spirv_cross_invoke(spirv_cross_shader_t *shader);

#ifdef ENABLE_JIT
#include "jit-engine.h"
#else
#include "dll-engine.h"
#endif

typedef struct
{
  int id;
  int set_no;
  int binding_no;
  std::string name;
} SSBO;

typedef struct spirv_cross_interface *(*spirv_cross_get_interface_fn)();

#define LOCAL_SIZE_X 16
#define LOCAL_SIZE_Y 16

#define WINDOW_SIZE 1024

inline static unsigned char fclamp(float x)
{
    int i = static_cast<int>(std::pow(x, 1.0f / 2.2f) * 256.0f); // simple gamma correction
    if (i > 255)
        i = 255;
    if (i < 0)
        i = 0;

    return static_cast<unsigned char>(i);
}

static void SaveImageAsPNG(const char *filename, const float *rgba, int width, int height)
{

    std::vector<unsigned char> ldr(static_cast<size_t>(width * height * 3));
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            // Flip Y
            ldr[3 * static_cast<size_t>((height - y - 1) * width + x) + 0] = fclamp(rgba[4 * (y * width + x) + 0]);
            ldr[3 * static_cast<size_t>((height - y - 1) * width + x) + 1] = fclamp(rgba[4 * (y * width + x) + 1]);
            ldr[3 * static_cast<size_t>((height - y - 1) * width + x) + 2] = fclamp(rgba[4 * (y * width + x) + 2]);
        }
    }

    int len = stbi_write_png(filename, width, height, 3, &ldr.at(0), width * 3);
    if (len < 1)
    {
        printf("Failed to save image\n");
        exit(-1);
    }
}

static bool exec_command(std::vector<std::string> *outputs, const std::string &cmd)
{
    outputs->clear();

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
        //printf("%s", buf);
        outputs->push_back(buf);
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
static bool compile_glsl(const std::string &output_filename, bool verbose, const std::string &glsl_filename)
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

    std::vector<std::string> outputs;
    bool ret = exec_command(&outputs, cmd);
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

// very simple resource parser for spirv-cross --dump-resources
static bool parse_ssbo(int *id, int *set_no, int *binding_no, std::string* name, const std::string& line)
{
  char buf[1024];

  std::cout << "parse: " << line << std::endl;
  int n = sscanf(line.c_str(), " ID %d : %s (Set : %d) (Binding : %d)", id, buf, set_no, binding_no);
  if (n == 4) {
    (*name) = std::string(buf);
    return true;
  } 

  return false;
}


// spirv -> c++
static bool compile_spirv(const std::string &output_filename, bool verbose, const std::string &spirv_filename)
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
    ss << " --dump-resources";
    ss << " 2>&1"; // spirv-cross dumps info to stderr. redirect stderr to stdout to catch dump info. 

    std::string cmd;
    cmd = ss.str();
    if (verbose)
    {
        std::cout << cmd << std::endl;
    }

    std::vector<std::string> outputs;
    bool ret = exec_command(&outputs, cmd);
    if (ret)
    {
        int parse_mode = 0;
        const int kSSBO = 1;
        for (size_t i = 0; i < outputs.size(); i++) {
          if (outputs[i].find("ssbos") != std::string::npos) {
            parse_mode = kSSBO;
          }

          if (parse_mode == kSSBO) {
            int id, set_no, binding_no;
            std::string name;
            bool ssbo_ret = parse_ssbo(&id, &set_no, &binding_no, &name, outputs[i]);
            if (ssbo_ret) {
              std::cout << "bingo!" << std::endl;
            }
          }
        }

        //
        // Check if compiled cpp code exists.
        //
        std::ifstream ifile(output_filename);
        if (!ifile)
        {
            std::cerr << "Failed to translate SPIR-V to C++" << std::endl;
            return false;
        }
      return true;
    }

    return false;

}

// c++ -> dll
static bool compile_cpp(const std::string &output_filename, const std::string &options, bool verbose,
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
    ss << " -I./glm"; // TODO(syoyo): User-supplied path to glm
    ss << " -I./SPIRV-Cross/include"; // TODO(syoyo): User-supplied path to SPIRV-Cross/include.
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

    std::vector<std::string> outputs;
    bool ret = exec_command(&outputs, cmd);
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
    std::chrono::duration<double, std::milli> compile_ms = end_time - start_time;
    std::cout << "compile time: " << compile_ms.count() << " ms" << std::endl;

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
                work_group_id.x = static_cast<unsigned int>(x);
                work_group_id.y = static_cast<unsigned int>(y);

                interface()->invoke(shader);
            }
        }
        auto t_end = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double, std::milli> exec_ms = t_end - t_begin;
        std::cout << "execute time: " << exec_ms.count() << " ms" << std::endl;
    }

    interface()->destruct(shader);

    // Save imag
    SaveImageAsPNG("output.png", &outbuf.at(0), WINDOW_SIZE, WINDOW_SIZE);

    std::cout << "output.png written." << std::endl;

    return EXIT_SUCCESS;
}
#endif

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

    return EXIT_SUCCESS;
}

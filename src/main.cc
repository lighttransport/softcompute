#include <cstdio>
#include <cstdlib>
#include <iostream>
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

#include "jit-engine.h"

typedef struct spirv_cross_interface *(*spirv_cross_get_interface_fn)();

#define LOCAL_SIZE_X  16
#define LOCAL_SIZE_Y  16

#define WINDOW_SIZE   512

inline unsigned char fclamp(float x) {
  int i = (int)(powf(x, 1.0 / 2.2) * 256.0f); // simple gamma correction
  if (i > 255)
    i = 255;
  if (i < 0)
    i = 0;

  return (unsigned char)i;
}

void SaveImageAsPNG(const char *filename, const float *rgba, int width,
                  int height) {

  std::vector<unsigned char> ldr(width * height * 3);
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      // Flip Y
      ldr[3*((height - y - 1) * width + x)+0] = fclamp(rgba[4*(y * width + x)+0]);
      ldr[3*((height - y - 1) * width + x)+1] = fclamp(rgba[4*(y * width + x)+1]);
      ldr[3*((height - y - 1) * width + x)+2] = fclamp(rgba[4*(y * width + x)+2]);
    }
  }

  int len = stbi_write_png(filename, width, height, 3, &ldr.at(0), width * 3);
  if (len < 1) {
    printf("Failed to save image\n");
    exit(-1);
  }
}

int main(int argc, char **argv)
{
	if (argc < 2)
	{
		std::cout << "Needs input cpp shader." << std::endl;
		return EXIT_FAILURE;
	}

	std::string filename = argv[1];

	softcompute::ShaderEngine engine;

	auto start_time = std::chrono::high_resolution_clock::now();

	std::vector<std::string> paths;
	softcompute::ShaderInstance *instance = engine.Compile("comp", /* id */ 0, paths, filename);

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
    for (int y = 0; y < WINDOW_SIZE / LOCAL_SIZE_Y; y++) {
      for (int x = 0; x < WINDOW_SIZE / LOCAL_SIZE_X; x++) {
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

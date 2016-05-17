#include <cstdio>
#include <cstdlib>
#include <vector>
#include <iostream>

#include <chrono>

#include <stdint.h>

#include "spirv_cross/external_interface.h"
#include "spirv_cross/internal_interface.hpp"

extern spirv_cross_shader_t* spirv_cross_construct(void);
extern void spirv_cross_destruct(spirv_cross_shader_t *shader);
extern void spirv_cross_invoke(spirv_cross_shader_t *shader);

#include "jit-engine.h"

typedef struct spirv_cross_interface *(*spirv_cross_get_interface_fn)();

int 
main(int argc, char **argv)
{
  //std::vector<float> inbuf(4*512);
  //std::vector<float> outbuf(4*512);
  //uint32_t counter = 0;

  //spirv_cross_shader_t *shader = spirv_cross_construct();

  //const float *in_ptr = &inbuf.at(0);
  //const float *out_ptr = &outbuf.at(0);
  //const uint32_t *counter_ptr = &counter;

  //std::vector<int> workGroupID(512);

  //spirv_cross_set_builtin(shader, SPIRV_CROSS_BUILTIN_WORK_GROUP_ID, &workGroupID.at(0), 512);

  //spirv_cross_set_resource(shader, 0, 0, (void **)&in_ptr, sizeof(void*));
  //spirv_cross_set_resource(shader, 0, 1, (void **)&out_ptr, sizeof(void*));
  //spirv_cross_set_resource(shader, 0, 2, (void **)&counter_ptr, sizeof(void*));

  //inbuf[0] = 3.14;
  //inbuf[1] = 3.14;
  //inbuf[2] = 3.14;
  //inbuf[3] = 3.14;

  //spirv_cross_invoke(shader);

  //printf("out = %f\n", outbuf[0]);

  //spirv_cross_destruct(shader);
  
  if (argc < 2) {
    std::cout << "Needs input cpp shader." << std::endl;
    return EXIT_FAILURE;
  }
  

  std::string filename = argv[1];

  softcompute::ShaderEngine engine;

  auto start_time = std::chrono::high_resolution_clock::now();

  std::vector<std::string> paths;
  softcompute::ShaderInstance* instance = engine.Compile("comp", /* id */0, paths, filename);

  if (!instance) {
    return EXIT_FAILURE;
  }

  auto end_time = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double, std::milli> ms = end_time - start_time;
  std::cout << "compile time: " << ms.count() << " ms" << std::endl;

  spirv_cross_get_interface_fn interface = reinterpret_cast<spirv_cross_get_interface_fn>(instance->GetInterface());

  //std::cout << interface << std::endl;
  //std::cout << interface()->construct << std::endl;

  spirv_cross_shader_t *shader = interface()->construct();
	//void (*destruct)(spirv_cross_shader_t *thiz);
	//void (*invoke)(spirv_cross_shader_t *thiz);

  std::vector<float> inbuf(4*512);
  std::vector<float> outbuf(4*512);
  uint32_t counter = 0;

  const float *in_ptr = &inbuf.at(0);
  const float *out_ptr = &outbuf.at(0);
  const uint32_t *counter_ptr = &counter;

  std::vector<int> workGroupID(512);

  spirv_cross_set_builtin(shader, SPIRV_CROSS_BUILTIN_WORK_GROUP_ID, &workGroupID.at(0), 512);

  spirv_cross_set_resource(shader, 0, 0, (void **)&in_ptr, sizeof(void*));
  spirv_cross_set_resource(shader, 0, 1, (void **)&out_ptr, sizeof(void*));
  spirv_cross_set_resource(shader, 0, 2, (void **)&counter_ptr, sizeof(void*));

  inbuf[0] = 3.14;
  inbuf[1] = 3.14;
  inbuf[2] = 3.14;
  inbuf[3] = 3.14;

  interface()->invoke(shader);

  std::cout << "out = " << outbuf[0] << ", " << outbuf[1] << ", " << outbuf[2] << ", " << outbuf[3] << "\n";

  interface()->destruct(shader);

  std::cout << shader << std::endl;

  return EXIT_SUCCESS;
}

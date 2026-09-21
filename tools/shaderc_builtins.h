#ifndef BGAME_SHADERC_BUILTINS_H
#define BGAME_SHADERC_BUILTINS_H

// CF's builtin shaders, from cute_framework/tools/builtin_shaders.h. That header spells them
// as C++ raw string literals, so shaderc_builtins.cpp includes it and hands them to C.

#include "cute_shader.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const char* const shaderc_shader_stub;
extern const char* const shaderc_draw_fs;
extern const char* const shaderc_blit_fs;

extern const CF_ShaderCompilerFile* const shaderc_builtin_includes;
extern const int shaderc_num_builtin_includes;

// Where the draw fragment shader's payload storage buffer binds for a preprocessed stub.
int
shaderc_compute_payload_binding(const char* shader);

#ifdef __cplusplus
}
#endif

#endif

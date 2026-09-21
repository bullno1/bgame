// The one C++ file of bgame-shaderc: see shaderc_builtins.h.

#include "shaderc_builtins.h"
#include "builtin_shaders.h"

extern "C" {

const char* const shaderc_shader_stub = s_shader_stub;
const char* const shaderc_draw_fs = s_draw_fs;
const char* const shaderc_blit_fs = s_blit_fs;

const CF_ShaderCompilerFile* const shaderc_builtin_includes = s_builtin_includes;
const int shaderc_num_builtin_includes = sizeof(s_builtin_includes) / sizeof(s_builtin_includes[0]);

int
shaderc_compute_payload_binding(const char* shader) {
	return cf_compute_payload_binding(shader);
}

}

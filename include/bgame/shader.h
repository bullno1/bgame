#ifndef BGAME_SHADER_H
#define BGAME_SHADER_H

#include <cute_graphics.h>
#include <cute_draw.h>

// Shader is special in the sense that it's both code and data.
// It's (re)compiled with cute-shaderc and trigger a hot code reload.
// But to reupload it to the GPU only on change requires a bit of special handling
void
bgame_load_draw_shader(CF_Shader* shader, CF_DrawShaderBytecode bytecode);

#endif

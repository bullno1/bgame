#ifndef BGAME_ASSET_SHADER_H
#define BGAME_ASSET_SHADER_H

#include <cute_draw.h>

struct bgame_asset_bundle_s;

struct CF_Shader
bgame_load_shader(
	struct bgame_asset_bundle_s* bundle,
	const char* path,
	const struct CF_DrawShaderBytecode* bytecode
);

#endif

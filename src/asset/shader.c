#include <bgame/asset.h>
#include <bgame/asset/shader.h>
#include <cute_draw.h>
#include <blog.h>

typedef struct {
	CF_Shader handle;
	const CF_DrawShaderBytecode* bytecode;
} bgame_shader_t;

static bgame_asset_load_result_t
bgame_shader_load(
	bgame_asset_bundle_t* bundle,
	void* asset,
	const char* path,
	const void* args
) {
	bgame_shader_t* shader = asset;
	if (bgame_asset_source_changed(bundle, shader)) {
		shader->bytecode = NULL;
	}
	if (shader->bytecode != NULL) {
		return BGAME_ASSET_UNCHANGED;
	}
	if (args == NULL) {
		return BGAME_ASSET_UNCHANGED;
	}

	CF_Shader compiled_shader = cf_make_draw_shader_from_bytecode(*(CF_DrawShaderBytecode*)args);
	if (compiled_shader.id != 0) {
		if (shader->handle.id != 0) {
			cf_destroy_shader(shader->handle);
		}

		shader->handle = compiled_shader;
		shader->bytecode = args;
		return BGAME_ASSET_LOADED;
	} else {
		return BGAME_ASSET_ERROR;
	}
}

static void
bgame_shader_unload(bgame_asset_bundle_t* bundle, void* asset) {
	bgame_shader_t* shader = asset;
	cf_destroy_shader(shader->handle);
	shader->handle.id = 0;
}

BGAME_ASSET_TYPE(shader) = {
	.name = "shader",
	.size = sizeof(bgame_shader_t),
	.load = bgame_shader_load,
	.unload = bgame_shader_unload,
};

struct CF_Shader
bgame_load_shader(
	struct bgame_asset_bundle_s* bundle,
	const char* path,
	const struct CF_DrawShaderBytecode* bytecode
) {
	bgame_shader_t* result = bgame_asset_load(bundle, &shader, path, bytecode);
	return result != NULL ? result->handle : (CF_Shader){ 0 };
}

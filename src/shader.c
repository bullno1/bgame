#include <bgame/shader.h>
#include <bgame/reloadable.h>
#include <bgame/allocator.h>
#include <bhash.h>
#include <blog.h>
#include "chibihash64.h"

#if BGAME_RELOADABLE

typedef BHASH_TABLE(CF_Shader, uint64_t) bgame_shader_bytecode_cache_t;

BGAME_VAR(bgame_shader_bytecode_cache_t, bgame_shader_bytecode_cache) = { 0 };

static void
bgame_init_shader_bytecode_cache(void) {
	bhash_config_t hconfig = bhash_config_default();
	hconfig.memctx = bgame_default_allocator;

	bhash_reinit(&bgame_shader_bytecode_cache, hconfig);
}

#endif

void
bgame_load_draw_shader(CF_Shader* shader, CF_DrawShaderBytecode bytecode) {
#if BGAME_RELOADABLE
	bgame_init_shader_bytecode_cache();

	uint64_t hash = chibihash64(bytecode.draw_shader.content, bytecode.draw_shader.size, 0);

	bhash_index_t index = bhash_find(&bgame_shader_bytecode_cache, *shader);
	if (bhash_is_valid(index)) {
		uint64_t cached_hash = bgame_shader_bytecode_cache.values[index];
		if (cached_hash != hash) {
			if (shader->id != 0) { cf_destroy_shader(*shader); }

			bhash_remove(&bgame_shader_bytecode_cache, *shader);
			*shader = cf_make_draw_shader_from_bytecode(bytecode);
			bhash_put(&bgame_shader_bytecode_cache, *shader, hash);
		}
	} else {
		if (shader->id != 0) { cf_destroy_shader(*shader); }

		*shader = cf_make_draw_shader_from_bytecode(bytecode);
		bhash_put(&bgame_shader_bytecode_cache, *shader, hash);
	}
#else
	*shader = cf_make_draw_shader_from_bytecode(bytecode);
#endif
}

#ifndef BGAME_UTILS_H
#define BGAME_UTILS_H

#include <cute_coroutine.h>
#include <bgame/reloadable.h>
#include <bent.h>
#include <barena.h>
#include <stdarg.h>
#include <bmacro.h>

static bent_t bgame__last_entity;

#define BGAME_SCOPE(ENTER, EXIT) \
	for ( \
		int BGAME_UNIQUE_VAR(bgame__scope_) = (ENTER, 0); \
		BGAME_UNIQUE_VAR(bgame__scope_) < 1; \
		++BGAME_UNIQUE_VAR(bgame__scope_), EXIT \
	)

#define BGAME_UNIQUE_VAR(PREFIX) BGAME_CONCAT(PREFIX, __LINE__)

#define BGAME_CONCAT(PREFIX, SUFFIX) BGAME__CONCAT2(PREFIX, SUFFIX)
#define BGAME__CONCAT2(PREFIX, SUFFIX) PREFIX##SUFFIX

#define BGAME_RUN_CORO_ON(condition, function, arg) \
	do { \
		static CF_Coroutine BGAME_UNIQUE_VAR = { 0 }; \
		if (condition) { bgame_spawn_coro(&BGAME_UNIQUE_VAR, function, arg); } \
		bgame_resume_coro(&BGAME_UNIQUE_VAR); \
	} while (0);

#define $this \
	bgame__this_world, bgame__this_entity

#define with_entity(WORLD, ENTITY) \
	for (bent_world_t* bgame__this_world = (WORLD); bgame__this_world != NULL; bgame__this_world = NULL) \
		for (bent_t bgame__this_entity = (ENTITY); bgame__this_entity.index != 0; bgame__this_entity.index = 0)

#define make_entity(WORLD) \
	bgame__last_entity = bent_create((WORLD)); \
	with_entity(WORLD, bgame__last_entity)

#ifdef BGAME_SCENE_NAME
#	include "scene.h"
#	include "allocator/tracked.h"
#	define SCENE BGAME_SCENE(BGAME_SCENE_NAME) =
#	define SCENE_VAR(TYPE, NAME) BGAME_SCENE_VAR(BGAME_SCENE_NAME, TYPE, NAME)
BGAME_DECLARE_SCENE_ALLOCATOR(BGAME_SCENE_NAME)
#endif

typedef struct {
	CF_Coroutine coro;
	bgame_reload_block_t reload_block;
} bgame_coro_t;

static inline void
bgame_spawn_coro_at(bgame_coro_t* coro, CF_CoroutineFn fn, void* arg, const char* file, int line) {
	if (coro->coro.id == 0) {
		coro->coro = cf_make_coroutine(fn, 0, arg);
		coro->reload_block = bgame_block_reload();
	}
}

static inline void
bgame_resume_coro(bgame_coro_t* coro) {
	if (coro->coro.id != 0) {
		cf_coroutine_resume(coro->coro);
		if (cf_coroutine_state(coro->coro) == CF_COROUTINE_STATE_DEAD) {
			bgame_unblock_reload(coro->reload_block);
			cf_destroy_coroutine(coro->coro);
			coro->coro.id = 0;
		}
	}
}

#define bgame_spawn_coro(CORO, FN) bgame_spawn_coro_at(CORO, FN, __FILE__, __LINE__)

static inline const char*
bgame_arena_vfmt(barena_t* arena, int* len_out, const char* fmt, va_list args) {
	char fmt_buf[512];
	va_list args_copy;
	va_copy(args_copy, args);
	int len = vsnprintf(fmt_buf, sizeof(fmt_buf), fmt, args_copy);

	char* result = barena_memalign(arena, len + 1, _Alignof(char));
	if (len >= (int)sizeof(fmt_buf)) {
		vsnprintf(result, len + 1, fmt, args);
	} else {
		memcpy(result, fmt_buf, len + 1);
	}

	va_end(args_copy);

	if (len_out != NULL) { *len_out = len; }
	return result;
}

BFORMAT_ATTRIBUTE(2, 3)
static inline const char*
bgame_arena_fmt(barena_t* arena, const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	const char* result = bgame_arena_vfmt(arena, NULL, fmt, args);
	va_end(args);
	return result;
}

static inline const char*
bgame_arena_strcpy(barena_t* arena, const char* str) {
	if (str == NULL) { return NULL; }

	size_t len = strlen(str);
	char* copy = barena_memalign(arena, len + 1, _Alignof(char));
	memcpy(copy, str, len + 1);
	return copy;
}

#endif

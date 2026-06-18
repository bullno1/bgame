#ifndef BGAME_UTILS_H
#define BGAME_UTILS_H

#include <cute_coroutine.h>
#include <bgame/reloadable.h>
#include <bent.h>

static bent_t bgame__last_entity;

#define BGAME_SCOPE(ENTER, EXIT) \
	for ( \
		int BGAME_UNIQUE_VAR(bgame__scope_) = (ENTER, 0); \
		BGAME_UNIQUE_VAR(bgame__scope_) < 1; \
		++BGAME_UNIQUE_VAR(bgame__scope_), EXIT \
	)

#define BGAME_CF_COLOR(color) \
	BGAME_SCOPE(cf_draw_push_color(color), cf_draw_pop_color())

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

#define make_entity(WORLD) \
	bgame__last_entity = bent_create(WORLD); \
	for (bent_world_t* bgame__this_world = WORLD; bgame__this_world != NULL; bgame__this_world = NULL) \
		for (bent_t bgame__this_entity = bgame__last_entity; bgame__this_entity.index != 0; bgame__this_entity.index = 0)

static inline void
bgame_spawn_coro(CF_Coroutine* coro, CF_CoroutineFn fn, void* arg) {
	if (coro->id == 0) {
		*coro = cf_make_coroutine(fn, 0, arg);
		bgame_block_reload();
	}
}

static inline void
bgame_resume_coro(CF_Coroutine* coro) {
	if (coro->id != 0) {
		cf_coroutine_resume(*coro);
		if (cf_coroutine_state(*coro) == CF_COROUTINE_STATE_DEAD) {
			bgame_unblock_reload();
			cf_destroy_coroutine(*coro);
			coro->id = 0;
		}
	}
}

#endif

#include "../internal.h"
#include <bgame/reloadable.h>
#include <bgame/allocator.h>
#include <bgame/allocator/frame.h>
#include <barena.h>

BGAME_VAR(barena_pool_t*, bgame_arena_pool) = NULL;
BGAME_VAR(barena_t*, bgame_current_arena) = NULL;
BGAME_VAR(barena_t*, bgame_previous_arena) = NULL;
BGAME_VAR(bgame_allocator_t*, bgame_frame_allocator) = NULL;

void*
bgame_alloc_for_frame(size_t size, size_t alignment) {
	return barena_memalign(bgame_current_arena, size, alignment);
}

void*
bgame_begin_frame_alloc(void) {
	return barena_snapshot(bgame_current_arena);
}

void
bgame_end_frame_alloc(void* marker) {
	barena_restore(bgame_current_arena, marker);
}

static void*
bgame_frame_allocator_realloc(void* ptr, size_t size, bgame_allocator_t* ctx) {
	if (ptr != NULL || size == 0) {
		return NULL;
	}

	return bgame_alloc_for_frame(size, _Alignof(BGAME_MAX_ALIGN_TYPE));
}

void
bgame_frame_allocator_init(void) {
	if (bgame_frame_allocator == NULL) {
		bgame_frame_allocator = bgame_malloc(sizeof(bgame_allocator_t), bgame_default_allocator);
	}
	bgame_frame_allocator->realloc = bgame_frame_allocator_realloc;

	if (bgame_arena_pool == NULL) {
		bgame_arena_pool = bgame_malloc(sizeof(barena_pool_t), bgame_default_allocator);
		bgame_current_arena = bgame_malloc(sizeof(barena_t), bgame_default_allocator);
		bgame_previous_arena = bgame_malloc(sizeof(barena_t), bgame_default_allocator);

		barena_pool_init(bgame_arena_pool, 32 * 1024);
		barena_init(bgame_current_arena, bgame_arena_pool);
		barena_init(bgame_previous_arena, bgame_arena_pool);
	}
}

void
bgame_frame_allocator_next_frame(void) {
	barena_t* tmp = bgame_current_arena;
	bgame_current_arena = bgame_previous_arena;
	bgame_previous_arena = tmp;
	barena_reset(bgame_current_arena);
}

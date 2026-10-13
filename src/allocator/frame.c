#include "../internal.h"
#include <bgame/reloadable.h>
#include <bgame/allocator.h>
#include <bgame/allocator/frame.h>
#include <barena.h>
#include <barray.h>
#include <bfmt_log.h>

BGAME_VAR(barena_pool_t*, bgame_arena_pool) = NULL;
BGAME_VAR(barena_t*, bgame_current_arena) = NULL;
BGAME_VAR(barena_t*, bgame_previous_arena) = NULL;
BGAME_VAR(bgame_allocator_t*, bgame_frame_allocator) = NULL;
BGAME_VAR(barray(char), bgame_str_buf) = NULL;
BGAME_VAR(int, bgame_str_depth) = 0;
// The write pointers are set on every begin, so these need not survive a reload
static bfmt_stream_t bgame_str_stream = { 0 };
static bfmt_stream_t bgame_str_null_stream = { 0 };

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

static void
bgame_str_write(void* userdata, const char* str, int len) {
	(void)userdata;
	size_t old_len = barray_len(bgame_str_buf);
	barray_resize(bgame_str_buf, old_len + (size_t)len, bgame_default_allocator);
	memcpy(bgame_str_buf + old_len, str, (size_t)len);
}

static void
bgame_str_null_write(void* userdata, const char* str, int len) {
	(void)userdata;
	(void)str;
	(void)len;
}

bfmt_stream_t*
bgame_begin_str(void) {
	if (bgame_str_depth++ > 0) {
		BFMT_ERROR("Nested bgame_print, dropping the inner one");
		bgame_str_null_stream.write = bgame_str_null_write;
		return &bgame_str_null_stream;
	}

	bgame_str_stream.write = bgame_str_write;
	return &bgame_str_stream;
}

bgame_str_t
bgame_end_str(void) {
	if (bgame_str_depth <= 0) {
		BFMT_ERROR("bgame_end_str without a begin");
		bgame_str_depth = 0;
		return (bgame_str_t){ .chars = "", .len = 0 };
	}

	if (--bgame_str_depth > 0) {
		return (bgame_str_t){ .chars = "", .len = 0 };
	}

	size_t len = barray_len(bgame_str_buf);
	char* copy = bgame_alloc_for_frame(len + 1, _Alignof(char));
	memcpy(copy, bgame_str_buf, len);
	copy[len] = '\0';
	barray_clear(bgame_str_buf);
	return (bgame_str_t){ .chars = copy, .len = (int)len };
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

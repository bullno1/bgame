#include "internal.h"
#include <bgame/reloadable.h>
#include <bgame/allocator.h>
#include <stdlib.h>

static void*
bgame_default_realloc(void* ptr, size_t size, bgame_allocator_t* ctx) {
	(void)ctx;
	if (size > 0) {
		return realloc(ptr, size);
	} else {
		free(ptr);
		return NULL;
	}
}

BGAME_VAR(bgame_allocator_t*, bgame_default_allocator) = NULL;

extern void
bgame_tracked_allocator_init(void);

extern void
bgame_frame_allocator_init(void);

extern void
bgame_cute_framework_allocator_init(void);

static void
bgame_allocator_init(void) {
	// Default allocator must be initialized first
	if (bgame_default_allocator == NULL) {
		bgame_default_allocator = bgame_default_realloc(NULL, sizeof(bgame_allocator_t), NULL);
	}
	bgame_default_allocator->realloc = bgame_default_realloc;

	bgame_cute_framework_allocator_init();
	bgame_tracked_allocator_init();
	bgame_frame_allocator_init();
}

BGAME_ON_LOAD(bgame_allocator) {
	bgame_allocator_init();
}

BGAME_AFTER_RELOAD(bgame_allocator) {
	bgame_allocator_init();
}

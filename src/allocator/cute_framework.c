#include <bgame/allocator.h>
#include <bgame/allocator/tracked.h>
#include <cute_alloc.h>
#include <string.h>

BGAME_DECLARE_TRACKED_ALLOCATOR(cute_framework)

static void*
bgame_cf_allocator_alloc(size_t size, void* udata) {
	(void)udata;
	return bgame_malloc(size, cute_framework);
}

static void
bgame_cf_allocator_free(void* ptr, void* udata) {
	(void)udata;
	bgame_free(ptr, cute_framework);
}

static void*
bgame_cf_allocator_calloc(size_t size, size_t count, void* udata) {
	(void)udata;
	void* mem = bgame_malloc(size * count, cute_framework);
	memset(mem, 0, size * count);
	return mem;
}

static void*
bgame_cf_allocator_realloc(void* ptr, size_t size, void* udata) {
	(void)udata;
	return bgame_realloc(ptr, size, cute_framework);
}

CF_Allocator bgame_cf_allocator = {
	.alloc_fn = bgame_cf_allocator_alloc,
	.free_fn = bgame_cf_allocator_free,
	.calloc_fn = bgame_cf_allocator_calloc,
	.realloc_fn = bgame_cf_allocator_realloc,
};

void
bgame_cute_framework_allocator_init(void) {
	cf_allocator_override(bgame_cf_allocator);
}

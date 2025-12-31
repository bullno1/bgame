#ifndef BGAME_ALLOCATOR_H
#define BGAME_ALLOCATOR_H

#include <bgame/reloadable.h>
#include <stddef.h>
#include <string.h>
#include <barena.h>

#ifdef _MSC_VER
#define BGAME_MAX_ALIGN_TYPE double
#else
#include <stddef.h>
#define BGAME_MAX_ALIGN_TYPE max_align_t
#endif

typedef struct bgame_allocator_s {
	void* (*realloc)(void* ptr, size_t size, struct bgame_allocator_s* ctx);
} bgame_allocator_t;

extern bgame_allocator_t* bgame_default_allocator;
extern barena_pool_t* bgame_arena_pool;

static inline void*
bgame_realloc(void* ptr, size_t size, bgame_allocator_t* allocator) {
	return allocator->realloc(ptr, size, allocator);
}

static inline void*
bgame_malloc(size_t size, bgame_allocator_t* allocator) {
	return bgame_realloc(NULL, size, allocator);
}

static inline void
bgame_free(void* ptr, bgame_allocator_t* allocator) {
	bgame_realloc(ptr, 0, allocator);
}

static inline void*
bgame_zalloc(size_t size, bgame_allocator_t* allocator) {
	void* mem = bgame_malloc(size, allocator);
	memset(mem, 0, size);
	return mem;
}

#endif

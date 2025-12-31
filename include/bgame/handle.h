#ifndef BGAME_HANDLE_H
#define BGAME_HANDLE_H

#include <stdint.h>
#include <stdbool.h>
#include <barray.h>

struct bgame_allocator_s;

#define BGAME_HANDLE_MAP_FOREACH(TYPE, VAR, MAP) \
	for ( \
		size_t bgame_handle_i = 0; \
		bgame_handle_i < barray_len((MAP)->slots); \
		++bgame_handle_i \
	) \
		for (TYPE VAR = ((MAP)->slots[bgame_handle_i]).ptr; VAR != NULL; VAR = NULL)

typedef struct {
	uint32_t index;
	uint32_t gen;
} bgame_handle_t;

typedef struct {
	uint32_t gen;
	uint32_t next;
	void* ptr;
} bgame_handle_slot_t;

typedef struct {
	struct bgame_allocator_s* allocator;
	barray(bgame_handle_slot_t) slots;
	uint32_t next_free;
} bgame_handle_map_t;

static inline bool
bgame_handle_is_invalid(bgame_handle_t handle) {
	// So that 0-init always give an invalid handle
	return handle.index == 0;
}

static inline bgame_handle_t
bgame_invalid_handle(void) {
	return (bgame_handle_t){ 0 };
}

void
bgame_handle_map_init(bgame_handle_map_t* handle_map, struct bgame_allocator_s* allocator);

void
bgame_handle_map_cleanup(bgame_handle_map_t* handle_map);

bgame_handle_t
bgame_handle_map_alloc(bgame_handle_map_t* handle_map, void* ptr);

void
bgame_handle_map_realloc(
	bgame_handle_map_t* handle_map,
	bgame_handle_t handle,
	void* new_ptr
);

void*
bgame_handle_map_resolve(bgame_handle_map_t* handle_map, bgame_handle_t handle);

void
bgame_handle_map_free(bgame_handle_map_t* handle_map, bgame_handle_t handle);

#endif

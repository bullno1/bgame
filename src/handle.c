#include <bgame/handle.h>

void
bgame_handle_map_init(bgame_handle_map_t* handle_map, struct bgame_allocator_s* allocator) {
	handle_map->allocator = allocator;
}

void
bgame_handle_map_cleanup(bgame_handle_map_t* handle_map) {
	if (handle_map->allocator) {
		barray_free(handle_map->slots, handle_map->allocator);
		handle_map->allocator = NULL;
		handle_map->slots = NULL;
	}
}

bgame_handle_t
bgame_handle_map_alloc(bgame_handle_map_t* handle_map, void* ptr) {
	if (handle_map->next_free == 0) {
		bgame_handle_slot_t slot = { .ptr = ptr };
		barray_push(handle_map->slots, slot, handle_map->allocator);
		return (bgame_handle_t){
			// Starting from 1 so that 0-initialized handles are always invalid
			.index = barray_len(handle_map->slots),
		};
	} else {
		uint32_t free_index = handle_map->next_free;
		bgame_handle_slot_t* slot = &handle_map->slots[free_index - 1];
		handle_map->next_free = slot->next;
		slot->ptr = ptr;

		return (bgame_handle_t){
			.index = free_index,
			.gen = slot->gen,
		};
	}
}

static bgame_handle_slot_t*
bgame_handle_map_resolve_slot(bgame_handle_map_t* handle_map, bgame_handle_t handle) {
	if (handle.index == 0) { return NULL; }
	if (handle.index > barray_len(handle_map->slots)) { return NULL; }

	bgame_handle_slot_t* slot = &handle_map->slots[handle.index - 1];
	if (slot->gen != handle.gen) { return NULL; }

	return slot;
}

void
bgame_handle_map_realloc(
	bgame_handle_map_t* handle_map,
	bgame_handle_t handle,
	void* new_ptr
) {
	bgame_handle_slot_t* slot = bgame_handle_map_resolve_slot(handle_map, handle);
	if (slot != NULL) { slot->ptr = new_ptr; }
}

void*
bgame_handle_map_resolve(bgame_handle_map_t* handle_map, bgame_handle_t handle) {
	bgame_handle_slot_t* slot = bgame_handle_map_resolve_slot(handle_map, handle);
	return slot != NULL ? slot->ptr : NULL;
}

void
bgame_handle_map_free(bgame_handle_map_t* handle_map, bgame_handle_t handle) {
	bgame_handle_slot_t* slot = bgame_handle_map_resolve_slot(handle_map, handle);
	if (slot == NULL) { return; }

	slot->gen += 1;
	slot->next = handle_map->next_free;
	slot->ptr = NULL;
	handle_map->next_free = handle.index;
}

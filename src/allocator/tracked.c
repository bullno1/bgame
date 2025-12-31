#include "../internal.h"
#include <bgame/allocator.h>
#include <bgame/allocator/tracked.h>
#include <stdatomic.h>

typedef struct {
	int_fast64_t size;
	_Alignas(BGAME_MAX_ALIGN_TYPE) char mem[];
} bgame_tracked_mem_t;

AUTOLIST_DEFINE(bgame_tracked_allocator_list)

typedef struct bgame_tracked_allocator_s {
	bgame_allocator_t allocator;

	atomic_int_fast64_t total;
	atomic_int_fast64_t peak;
} bgame_tracked_allocator_t;

void
bgame_enumerate_tracked_allocators(
	void (*fn)(const char* name, bgame_allocator_stats_t stats, void* userdata),
	void* userdata
) {
	AUTOLIST_FOREACH(entry, bgame_tracked_allocator_list) {
		bgame_tracked_allocator_t** allocator_ptr = entry->value_addr;
		if (*allocator_ptr == NULL) { continue; }

		bgame_allocator_stats_t stats = {
			.peak = (size_t)atomic_load_explicit(&(*allocator_ptr)->peak, memory_order_relaxed),
			.total = (size_t)atomic_load_explicit(&(*allocator_ptr)->total, memory_order_relaxed),
		};
		fn(entry->name, stats, userdata);
	}
}

static void
bgame_tracked_allocator_adjust(bgame_tracked_allocator_t* allocator, int_fast64_t change) {
	int_fast64_t total = atomic_fetch_add(&allocator->total, change) + change;

	if (change > 0) {
		int_fast64_t peak = atomic_load(&allocator->peak);

		while (total > peak) {
			if (atomic_compare_exchange_weak(&allocator->peak, &peak, total)) {
				break;
			}
		}
	}
}

static void*
bgame_tracked_allocator_realloc(void* ptr, size_t size, bgame_allocator_t* ctx) {
	bgame_tracked_allocator_t* allocator = (bgame_tracked_allocator_t*)ctx;

	if (ptr == NULL) {
		if (size == 0) { return NULL; }

		bgame_tracked_mem_t* mem = bgame_realloc(NULL, size + sizeof(bgame_tracked_mem_t), bgame_default_allocator);
		mem->size = (int_fast64_t)size;
		bgame_tracked_allocator_adjust(allocator, (int_fast64_t)size);
		return mem->mem;
	} else {
		bgame_tracked_mem_t* mem = (void*)((char*)ptr - offsetof(bgame_tracked_mem_t, mem));
		int_fast64_t old_size = mem->size;

		if (size == 0) {
			bgame_realloc(mem, 0, bgame_default_allocator);
			bgame_tracked_allocator_adjust(allocator, -old_size);
			return NULL;
		} else {
			bgame_tracked_mem_t* new_mem = bgame_realloc(mem, size + sizeof(bgame_tracked_mem_t), bgame_default_allocator);
			new_mem->size = (int_fast64_t)size;
			bgame_tracked_allocator_adjust(allocator, (int_fast64_t)size - old_size);
			return new_mem->mem;
		}
	}
}

void
bgame_tracked_allocator_init(void) {
	AUTOLIST_FOREACH(entry, bgame_tracked_allocator_list) {
		bgame_tracked_allocator_t** allocator_ptr = entry->value_addr;

		if (*allocator_ptr == NULL) {
			bgame_tracked_allocator_t* allocator = bgame_malloc(
				sizeof(bgame_tracked_allocator_t), bgame_default_allocator
			);

			*allocator = (bgame_tracked_allocator_t){
				.allocator.realloc = bgame_tracked_allocator_realloc,
			};

			*allocator_ptr = allocator;
		} else {
			(*allocator_ptr)->allocator.realloc = bgame_tracked_allocator_realloc;
		}
	}
}

#ifndef BGAME_TRACKED_ALLOCATOR_H

#include <bgame/reloadable.h>
#include <autolist.h>

#define BGAME_DECLARE_TRACKED_ALLOCATOR(NAME) \
	static struct bgame_allocator_s* NAME = NULL; \
	AUTOLIST_ADD_ENTRY(bgame_tracked_allocator_list, NAME, NAME) \
	BGAME_PERSIST_VAR(NAME)

#define BGAME_DECLARE_SCENE_ALLOCATOR(SCENE_NAME) \
	BGAME_PRIVATE_VAR(SCENE_NAME, struct bgame_allocator_s*, scene_allocator) \
	AUTOLIST_ADD_ENTRY(bgame_tracked_allocator_list, SCENE_NAME##_allocator, scene_allocator) \

struct bgame_allocator_s;

typedef struct bgame_allocator_stats_s {
	size_t total;
	size_t peak;
} bgame_allocator_stats_t;

void
bgame_enumerate_tracked_allocators(
	void (*fn)(const char* name, bgame_allocator_stats_t stats, void* userdata),
	void* userdata
);

#endif

#ifndef BGAME_SCENE_H
#define BGAME_SCENE_H

#include <autolist.h>
#include <bmacro.h>
#include <stdbool.h>

typedef enum {
	BGAME_SCENE_INITIALIZING,
	BGAME_SCENE_REINITIALIZING,
	BGAME_SCENE_RUNNING,
	BGAME_SCENE_SUSPENDING,
	BGAME_SCENE_RESUMING,
	BGAME_SCENE_CLEANING_UP,
} bgame_scene_state_t;

typedef struct bgame_scene_s {
	void (*init)(void);
	void (*update)(void);
	void (*cleanup)(void);
	void (*suspend)(void);
	void (*resume)(void);
	void (*before_reload)(void);
	void (*after_reload)(void);
} bgame_scene_t;

#define BGAME_SCENE(NAME) \
	AUTOLIST_ENTRY_EX(bgame_scene_list, bgame_scene_t, NAME, BCONCAT(bgame_scene_, NAME))

void
bgame_set_next_scene_data(void* data);

void
bgame_switch_scene(const char* name);

void
bgame_push_scene(const char* name);

void
bgame_pop_scene(void);

void
bgame_clear_scene_stack(void);

bgame_scene_t*
bgame_current_scene(void);

void*
bgame_current_scene_data(void);

bgame_scene_state_t
bgame_current_scene_state(void);

int
bgame_scene_stack_depth(void);

void
bgame_scene_update(void);

void
bgame_scene_before_reload(void);

void
bgame_scene_after_reload(void);

#endif

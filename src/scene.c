#include <bgame/scene.h>
#include <bgame/reloadable.h>
#include <cute_string.h>
#include <cute_app.h>
#include <blog.h>
#include <string.h>
#include <stdlib.h>

#ifndef BGAME_SCENE_STACK_SIZE
#define BGAME_SCENE_STACK_SIZE 8
#endif

typedef struct {
	void* data;
	const char* name;
} bgame_scene_entry_t;

typedef enum {
	BGAME_RUN_SCENE,
	BGAME_SWITCH_SCENE,
	BGAME_PUSH_SCENE,
	BGAME_POP_SCENE,
} bgame_scene_op_t;

static struct {
	int current_scene_index;
	bgame_scene_entry_t scene_stack[BGAME_SCENE_STACK_SIZE];

	bgame_scene_op_t scene_op;
	bgame_scene_state_t scene_state;
	bgame_scene_entry_t next_scene;
} bgame_scene_mgr = { 0 };

BGAME_PERSIST_VAR(bgame_scene_mgr)

AUTOLIST_DEFINE(bgame_scene_list)

static inline bgame_scene_t*
bgame_find_scene(const char* name) {
	if (name == NULL) { return NULL; }
	size_t name_len = strlen(name);
	AUTOLIST_FOREACH(entry, bgame_scene_list) {
		if (
			entry->name_length == name_len
			&& strncmp(entry->name, name, entry->name_length) == 0
		) {
			return entry->value_addr;
		}
	}

	return NULL;
}

void
bgame_set_next_scene_data(void* data) {
	bgame_scene_mgr.next_scene.data = data;
}

void
bgame_switch_scene(const char* name) {
	bgame_scene_t* scene = bgame_find_scene(name);
	if (scene == NULL) {
		BLOG_ERROR("Could not find scene: %s", name);
		return;
	}

	bgame_scene_mgr.next_scene.name = sintern(name);
	bgame_scene_mgr.scene_op = BGAME_SWITCH_SCENE;
}

void
bgame_reload_scene(void) {
	bgame_scene_entry_t* current_entry = &bgame_scene_mgr.scene_stack[bgame_scene_mgr.current_scene_index];
	bgame_scene_mgr.next_scene.name = current_entry->name;
	bgame_scene_mgr.scene_op = BGAME_SWITCH_SCENE;
}

void
bgame_push_scene(const char* name) {
	if (bgame_scene_mgr.current_scene_index >= BGAME_SCENE_STACK_SIZE - 1) {
		BLOG_ERROR("Scene stack is full");
		return;
	}

	bgame_scene_t* scene = bgame_find_scene(name);
	if (scene == NULL) {
		BLOG_ERROR("Could not find scene: %s", name);
		return;
	}

	bgame_scene_mgr.next_scene.name = sintern(name);
	bgame_scene_mgr.scene_op = BGAME_PUSH_SCENE;
}

void
bgame_pop_scene(void) {
	bgame_scene_mgr.scene_op = BGAME_POP_SCENE;
}

bgame_scene_t*
bgame_current_scene(void) {
	bgame_scene_entry_t* current_entry = &bgame_scene_mgr.scene_stack[bgame_scene_mgr.current_scene_index];
	return bgame_find_scene(current_entry->name);
}

void*
bgame_current_scene_data(void) {
	bgame_scene_entry_t* current_entry = &bgame_scene_mgr.scene_stack[bgame_scene_mgr.current_scene_index];
	return current_entry->data;
}

bgame_scene_state_t
bgame_current_scene_state(void) {
	return bgame_scene_mgr.scene_state;
}

static void
bgame_scene_update_internal(bool run_update) {
	bgame_scene_entry_t* current_entry = &bgame_scene_mgr.scene_stack[bgame_scene_mgr.current_scene_index];
	bgame_scene_t* current_scene = bgame_find_scene(current_entry->name);

	switch (bgame_scene_mgr.scene_op) {
		case BGAME_RUN_SCENE:
			// Update later
			break;
		case BGAME_SWITCH_SCENE: {
			if (current_scene != NULL && current_scene->cleanup != NULL) {
				BLOG_INFO("Cleaning up scene `%s`", current_entry->name);
				bgame_scene_mgr.scene_state = BGAME_SCENE_CLEANING_UP;
				current_scene->cleanup();
			}

			*current_entry = bgame_scene_mgr.next_scene;
			current_scene = bgame_find_scene(bgame_scene_mgr.next_scene.name);
			bgame_scene_mgr.next_scene = (bgame_scene_entry_t){ 0 };

			if (current_scene != NULL && current_scene->init != NULL) {
				BLOG_INFO("Initializing scene `%s`", current_entry->name);
				bgame_scene_mgr.scene_state = BGAME_SCENE_INITIALIZING;
				current_scene->init();
			}

			bgame_scene_mgr.scene_state = BGAME_SCENE_RUNNING;
			bgame_scene_mgr.scene_op = BGAME_RUN_SCENE;
		} break;
		case BGAME_PUSH_SCENE: {
			if (current_scene != NULL && current_scene->suspend != NULL) {
				BLOG_INFO("Suspending scene `%s`", current_entry->name);
				bgame_scene_mgr.scene_state = BGAME_SCENE_SUSPENDING;
				current_scene->suspend();
			}

			current_entry = &bgame_scene_mgr.scene_stack[++bgame_scene_mgr.current_scene_index];
			*current_entry = bgame_scene_mgr.next_scene;
			current_scene = bgame_find_scene(bgame_scene_mgr.next_scene.name);
			bgame_scene_mgr.next_scene = (bgame_scene_entry_t){ 0 };

			if (current_scene != NULL && current_scene->init != NULL) {
				BLOG_INFO("Initializing scene `%s`", current_entry->name);
				bgame_scene_mgr.scene_state = BGAME_SCENE_INITIALIZING;
				current_scene->init();
			}

			bgame_scene_mgr.scene_state = BGAME_SCENE_RUNNING;
			bgame_scene_mgr.scene_op = BGAME_RUN_SCENE;
		} break;
		case BGAME_POP_SCENE: {
			if (current_scene != NULL && current_scene->cleanup != NULL) {
				BLOG_INFO("Cleaning up scene `%s`", current_entry->name);
				bgame_scene_mgr.scene_state = BGAME_SCENE_CLEANING_UP;
				current_scene->cleanup();

				*current_entry = (bgame_scene_entry_t){ 0 };
			}

			if (bgame_scene_mgr.current_scene_index > 0) {
				current_entry = &bgame_scene_mgr.scene_stack[--bgame_scene_mgr.current_scene_index];
			}
			current_scene = bgame_find_scene(current_entry->name);

			if (current_scene != NULL && current_scene->resume != NULL) {
				BLOG_INFO("Resuming scene `%s`", current_entry->name);
				bgame_scene_mgr.scene_state = BGAME_SCENE_RESUMING;
				current_scene->resume();
			}

			bgame_scene_mgr.scene_state = BGAME_SCENE_RUNNING;
			bgame_scene_mgr.scene_op = BGAME_RUN_SCENE;
		} break;
	}

	if (run_update) {
		if (current_scene != NULL && current_scene->update != NULL) {
			current_scene->update();
		} else {
			cf_app_update(NULL);
			cf_app_draw_onto_screen(true);
		}
	}
}

void
bgame_scene_update(void) {
	bgame_scene_update_internal(true);
}

void
bgame_scene_before_reload(void) {
	bgame_scene_entry_t* current_entry = &bgame_scene_mgr.scene_stack[bgame_scene_mgr.current_scene_index];
	bgame_scene_t* current_scene = bgame_find_scene(current_entry->name);
	if (current_scene != NULL && current_scene->before_reload != NULL) {
		BLOG_INFO("Saving scene `%s`", current_entry->name);
		current_scene->before_reload();
	}
}

void
bgame_scene_after_reload(void) {
	bgame_scene_entry_t* current_entry = &bgame_scene_mgr.scene_stack[bgame_scene_mgr.current_scene_index];

	bgame_scene_t* current_scene = bgame_find_scene(current_entry->name);
	if (current_scene != NULL && current_scene->after_reload != NULL) {
		BLOG_INFO("Loading scene `%s`", current_entry->name);
		current_scene->after_reload();
	}

	if (current_scene != NULL && current_scene->init != NULL) {
		BLOG_INFO("Reinitializing scene `%s`", current_entry->name);
		bgame_scene_mgr.scene_state = BGAME_SCENE_REINITIALIZING;
		current_scene->init();
	}
}

void
bgame_clear_scene_stack(void) {
	while (bgame_current_scene() != NULL) {
		bgame_pop_scene();
		bgame_scene_update_internal(false);
	}
}

int
bgame_scene_stack_depth(void) {
	return bgame_scene_mgr.current_scene_index;
}

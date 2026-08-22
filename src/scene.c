#include <bgame/scene.h>
#include <bgame/reloadable.h>
#include <bgame/allocator/frame.h>
#include <cute_string.h>
#include <cute_app.h>
#include <blog.h>
#include <string.h>
#include <stdlib.h>

#ifndef BGAME_SCENE_STACK_SIZE
#define BGAME_SCENE_STACK_SIZE 8
#endif

typedef struct bgame_scene_entry_s {
	const char* name;
	void* data;
} bgame_scene_entry_t;

typedef enum {
	BGAME_RUN_SCENE,
	BGAME_SWITCH_SCENE,
	BGAME_PUSH_SCENE,
	BGAME_POP_SCENE,
} bgame_scene_op_t;

AUTOLIST_DEFINE(bgame_scene_list)
AUTOLIST_DEFINE(bgame_scene_var_list)

BGAME_SCENE(bgame__empty) = { 0 };

static struct {
	// Index of the current scene in the scene stack
	int current_scene_index;
	// Stack of scene name and data, used for navigation
	bgame_scene_entry_t scene_stack[BGAME_SCENE_STACK_SIZE];

	// Cached current scene registration, refreshed on hot reload
	bgame_scene_reg_t current_scene;
	// State of the current scene
	bgame_scene_state_t scene_state;
	// Pending scene change op
	bgame_scene_op_t scene_op;
	// Pending next scene target
	bgame_scene_entry_t next_scene;
} bgame_scene_mgr = {
	.current_scene.def = &bgame_scene_bgame__empty,
};
BGAME_PERSIST_VAR(bgame_scene_mgr)

static bgame_scene_reg_t
bgame_empty_scene(void) {
	return (bgame_scene_reg_t){
		.name = sintern("bgame__empty"),
		.def = &bgame_scene_bgame__empty,
	};
}

static bgame_scene_reg_t
bgame_find_scene(const char* name) {
	if (name == NULL) {
		return bgame_empty_scene();
	}

	size_t name_len = strlen(name);
	AUTOLIST_FOREACH(entry, bgame_scene_list) {
		if (
			entry->name_length == name_len
			&& strncmp(entry->name, name, entry->name_length) == 0
		) {
			return (bgame_scene_reg_t){
				.name = sintern(entry->name),
				.def = entry->value_addr,
			};
		}
	}

	BLOG_ERROR("Could not find scene: %s", name);
	return bgame_empty_scene();
}

static void
bgame_zero_scene_vars(const char* scene_name) {
	AUTOLIST_FOREACH(entry, bgame_scene_var_list) {
		const bgame_scene_var_t* var = entry->value_addr;
		if (strcmp(var->scene, scene_name) == 0) {
			memset(var->addr, 0, var->size);
		}
	}
}

void
bgame_set_next_scene_data(void* data) {
	bgame_scene_mgr.next_scene.data = data;
}

void
bgame_switch_scene(const char* name) {
	bgame_scene_mgr.next_scene.name = name;
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

	bgame_scene_mgr.next_scene.name = name;
	bgame_scene_mgr.scene_op = BGAME_PUSH_SCENE;
}

void
bgame_pop_scene(void) {
	bgame_scene_mgr.scene_op = BGAME_POP_SCENE;
}

bgame_scene_reg_t
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
	bgame_scene_reg_t current_scene = bgame_scene_mgr.current_scene;

	switch (bgame_scene_mgr.scene_op) {
		case BGAME_RUN_SCENE:
			// Update later
			break;
		case BGAME_SWITCH_SCENE: {
			// Cleanup old scene
			if (current_scene.def->cleanup != NULL) {
				BLOG_INFO("Cleaning up scene `%s`", current_scene.name);
				bgame_scene_mgr.scene_state = BGAME_SCENE_CLEANING_UP;
				current_scene.def->cleanup();
			}
			bgame_zero_scene_vars(current_scene.name);

			// Change current scene
			*current_entry = bgame_scene_mgr.next_scene;
			bgame_scene_mgr.next_scene = (bgame_scene_entry_t){ 0 };

			current_scene = bgame_scene_mgr.current_scene = bgame_find_scene(current_entry->name);
			current_entry->name = current_scene.name;
			BLOG_INFO("Switching to scene `%s`", current_scene.name);

			// Init new scene
			if (current_scene.def->init != NULL) {
				BLOG_INFO("Initializing scene `%s`", current_entry->name);
				bgame_scene_mgr.scene_state = BGAME_SCENE_INITIALIZING;
				current_scene.def->init();
			}
		} break;
		case BGAME_PUSH_SCENE: {
			// Suspend previous scene
			if (current_scene.def->suspend != NULL) {
				BLOG_INFO("Suspending scene `%s`", current_entry->name);
				bgame_scene_mgr.scene_state = BGAME_SCENE_SUSPENDING;
				current_scene.def->suspend();
			}

			// Change current scene
			current_entry = &bgame_scene_mgr.scene_stack[++bgame_scene_mgr.current_scene_index];
			*current_entry = bgame_scene_mgr.next_scene;
			bgame_scene_mgr.next_scene = (bgame_scene_entry_t){ 0 };

			current_scene = bgame_scene_mgr.current_scene = bgame_find_scene(current_entry->name);
			current_entry->name = current_scene.name;
			BLOG_INFO("Switching to scene `%s`", current_scene.name);

			// Init new scene
			if (current_scene.def->init != NULL) {
				BLOG_INFO("Initializing scene `%s`", current_entry->name);
				bgame_scene_mgr.scene_state = BGAME_SCENE_INITIALIZING;
				current_scene.def->init();
			}
		} break;
		case BGAME_POP_SCENE: {
			// Cleanup current scene
			if (current_scene.def->cleanup != NULL) {
				BLOG_INFO("Cleaning up scene `%s`", current_entry->name);
				bgame_scene_mgr.scene_state = BGAME_SCENE_CLEANING_UP;
				current_scene.def->cleanup();
			}
			bgame_zero_scene_vars(current_scene.name);
			*current_entry = (bgame_scene_entry_t){ 0 };

			// Move back to previous scene
			if (bgame_scene_mgr.current_scene_index > 0) {
				current_entry = &bgame_scene_mgr.scene_stack[--bgame_scene_mgr.current_scene_index];
			}

			current_scene = bgame_scene_mgr.current_scene = bgame_find_scene(current_entry->name);
			BLOG_INFO("Switching to scene `%s`", current_scene.name);

			// Resume previous scene
			if (current_scene.def->resume != NULL) {
				BLOG_INFO("Resuming scene `%s`", current_entry->name);
				bgame_scene_mgr.scene_state = BGAME_SCENE_RESUMING;
				current_scene.def->resume();
			}
		} break;
	}

	// Clear pending op
	bgame_scene_mgr.scene_state = BGAME_SCENE_RUNNING;
	bgame_scene_mgr.scene_op = BGAME_RUN_SCENE;

	// Regular update
	if (run_update) {
		if (current_scene.def->update != NULL) {
			current_scene.def->update();
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
	bgame_scene_reg_t current_scene = bgame_scene_mgr.current_scene;
	if (current_scene.def->before_reload != NULL) {
		BLOG_INFO("Saving scene `%s`", current_scene.name);
		current_scene.def->before_reload();
	}
}

void
bgame_scene_after_reload(void) {
	bgame_scene_entry_t* current_entry = &bgame_scene_mgr.scene_stack[bgame_scene_mgr.current_scene_index];
	bgame_scene_reg_t current_scene = bgame_scene_mgr.current_scene = bgame_find_scene(current_entry->name);

	if (current_scene.def->after_reload != NULL) {
		BLOG_INFO("Loading scene `%s`", current_entry->name);
		current_scene.def->after_reload();
	}

	if (current_scene.def->init != NULL) {
		BLOG_INFO("Reinitializing scene `%s`", current_entry->name);
		bgame_scene_mgr.scene_state = BGAME_SCENE_REINITIALIZING;
		current_scene.def->init();
	}
}

void
bgame_clear_scene_stack(void) {
	while (bgame_current_scene().def != &bgame_scene_bgame__empty) {
		bgame_pop_scene();
		bgame_scene_update_internal(false);
	}
}

int
bgame_scene_stack_depth(void) {
	return bgame_scene_mgr.current_scene_index;
}

void
bgame_list_scenes(bgame_scene_reg_t** scenes_out, int* num_scenes_out) {
	int num_scenes = 0;
	AUTOLIST_FOREACH(entry, bgame_scene_list) {
		if (entry->value_addr == &bgame_scene_bgame__empty) { continue; }

		++num_scenes;
	}

	if (num_scenes_out != NULL) {
		*num_scenes_out= num_scenes;
	}

	if (scenes_out != NULL) {
		*scenes_out = bgame_alloc_for_frame(sizeof(bgame_scene_reg_t) * num_scenes, _Alignof(bgame_scene_reg_t));

		int scene_index = 0;
		AUTOLIST_FOREACH(entry, bgame_scene_list) {
			if (entry->value_addr == &bgame_scene_bgame__empty) { continue; }

			(*scenes_out)[scene_index++] = (bgame_scene_reg_t){
				.name = sintern(entry->name),
				.def = entry->value_addr,
			};
		}
	}
}

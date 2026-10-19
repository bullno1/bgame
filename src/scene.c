#include <bgame/scene.h>
#include <bgame/reloadable.h>
#include <bgame/allocator.h>
#include <bgame/allocator/frame.h>
#include <cute_string.h>
#include <cute_app.h>
#include <bfmt_log.h>
#include <blog.h>
#include <string.h>
#include <stdlib.h>

#ifndef BGAME_SCENE_STACK_SIZE
#define BGAME_SCENE_STACK_SIZE 8
#endif

typedef struct bgame_scene_entry_s {
	const char* name;
	size_t arg_size;
	_Alignas(bgame_max_align_t) char arg[BGAME_SCENE_ARG_MAX];
} bgame_scene_entry_t;

static const _Alignas(bgame_max_align_t) char bgame_scene_zero_arg[BGAME_SCENE_ARG_MAX] = { 0 };

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

static bgame_scene_reg_t
bgame_find_scene_by_def(const bgame_scene_def_t* def) {
	AUTOLIST_FOREACH(entry, bgame_scene_list) {
		if (entry->value_addr == def) {
			return (bgame_scene_reg_t){
				.name = sintern(entry->name),
				.def = entry->value_addr,
			};
		}
	}

	BLOG_ERROR("Scene is not registered");
	return bgame_empty_scene();
}

// Index of the entry named so in the stack, -1 when none
static int
bgame_find_scene_in_stack(const char* name) {
	if (name == NULL) { return -1; }
	for (int i = 0; i <= bgame_scene_mgr.current_scene_index; ++i) {
		const char* entry_name = bgame_scene_mgr.scene_stack[i].name;
		if (entry_name != NULL && strcmp(entry_name, name) == 0) {
			return i;
		}
	}
	return -1;
}

static void
bgame_cleanup_scene(bgame_scene_reg_t scene) {
	if (scene.def->cleanup != NULL) {
		BLOG_INFO("Cleaning up scene `%s`", scene.name);
		bgame_scene_mgr.scene_state = BGAME_SCENE_CLEANING_UP;
		scene.def->cleanup();
	}
	bgame_zero_scene_vars(scene.name);
}

static void
bgame_set_next_scene(const char* name, bgame_scene_arg_t arg, bgame_scene_op_t op) {
	bgame_scene_entry_t* next = &bgame_scene_mgr.next_scene;
	*next = (bgame_scene_entry_t){ .name = name };
	if (arg.size > BGAME_SCENE_ARG_MAX) {
		BFMT_ERROR("Argument for scene ", name, " is ", arg.size, " bytes, dropped");
	} else if (arg.size > 0) {
		memcpy(next->arg, arg.data, arg.size);
		next->arg_size = arg.size;
	}
	bgame_scene_mgr.scene_op = op;
}

void
bgame__switch_scene(const bgame_scene_def_t* def, bgame_scene_arg_t arg) {
	bgame_set_next_scene(bgame_find_scene_by_def(def).name, arg, BGAME_SWITCH_SCENE);
}

void
bgame_switch_scene_by_name(const char* name) {
	bgame_set_next_scene(name, (bgame_scene_arg_t){ 0 }, BGAME_SWITCH_SCENE);
}

void
bgame_reload_scene(void) {
	bgame_scene_mgr.next_scene = bgame_scene_mgr.scene_stack[bgame_scene_mgr.current_scene_index];
	bgame_scene_mgr.scene_op = BGAME_SWITCH_SCENE;
}

static bool
bgame_can_push_scene(const char* name) {
	if (bgame_scene_mgr.current_scene_index >= BGAME_SCENE_STACK_SIZE - 1) {
		BLOG_ERROR("Scene stack is full");
		return false;
	}
	if (bgame_find_scene_in_stack(name) >= 0) {
		BFMT_ERROR("Scene ", name, " is already in the stack, not pushed");
		return false;
	}
	return true;
}

void
bgame__push_scene(const bgame_scene_def_t* def, bgame_scene_arg_t arg) {
	const char* name = bgame_find_scene_by_def(def).name;
	if (!bgame_can_push_scene(name)) { return; }
	bgame_set_next_scene(name, arg, BGAME_PUSH_SCENE);
}

void
bgame_push_scene_by_name(const char* name) {
	if (!bgame_can_push_scene(name)) { return; }
	bgame_set_next_scene(name, (bgame_scene_arg_t){ 0 }, BGAME_PUSH_SCENE);
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

const void*
bgame__scene_arg(const bgame_scene_def_t* def, size_t size) {
	bgame_scene_entry_t* current_entry = &bgame_scene_mgr.scene_stack[bgame_scene_mgr.current_scene_index];
	if (bgame_scene_mgr.current_scene.def != def) {
		BFMT_ERROR("Scene ", current_entry->name, " asked for another scene's argument");
		return bgame_scene_zero_arg;
	}
	if (current_entry->arg_size != 0 && current_entry->arg_size != size) {
		BFMT_ERROR(
			"Scene ", current_entry->name, " was given ", current_entry->arg_size,
			" bytes of argument, expects ", size
		);
		return bgame_scene_zero_arg;
	}
	return current_entry->arg;
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
			// A scene lower in the stack is unwound to, so it never runs twice
			int target_index = bgame_find_scene_in_stack(bgame_scene_mgr.next_scene.name);
			while (target_index >= 0 && bgame_scene_mgr.current_scene_index > target_index) {
				bgame_cleanup_scene(current_scene);
				*current_entry = (bgame_scene_entry_t){ 0 };
				current_entry = &bgame_scene_mgr.scene_stack[--bgame_scene_mgr.current_scene_index];
				current_scene = bgame_scene_mgr.current_scene = bgame_find_scene(current_entry->name);
			}

			// Cleanup old scene
			bgame_cleanup_scene(current_scene);

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
			bgame_cleanup_scene(current_scene);
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
bgame_scene_check_reload(void) {
	bgame_scene_reg_t current_scene = bgame_scene_mgr.current_scene;
	if (current_scene.def->check_reload != NULL) {
		current_scene.def->check_reload();
	}
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

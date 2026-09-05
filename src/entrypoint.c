#include "internal.h"
#include <bgame/reloadable.h>
#include <bgame/allocator.h>
#include <blog.h>
#include "loader_interface.h"

typedef struct {
	const char* file;
	int line;
} bgame_reload_blocker_t;

extern void
bgame_frame_allocator_next_frame(void);

const char* bgame_entry_file = __FILE__;

#if BGAME_RELOADABLE

static int bgame_reload_block_counter = 0;
static bgame_loader_interface_t* bgame_loader_interface = NULL;
static bgame_handle_map_t bgame_reload_blockers = { 0 };

static void
bgame_update(bgame_loader_interface_t* interface) {
	bgame_frame_allocator_next_frame();
	interface->app.update();
}

static bool
bgame_is_reload_blocked(bgame_loader_interface_t* interface) {
	return bgame_reload_block_counter > 0;
}

static void
bgame_explain_reload_blocked(bgame_loader_interface_t* interface) {
	BGAME_HANDLE_MAP_FOREACH(bgame_reload_blocker_t, blocker, &bgame_reload_blockers) {
		blog_write(
			BLOG_LEVEL_DEBUG, blocker->file, blocker->line,
			"<-- Blocking reload"
		);
	}
}

bgame_reload_block_t
bgame_block_reload_at(const char* file, int line) {
	bgame_reload_blocker_t* blocker = bgame_malloc(sizeof(bgame_reload_blocker_t), bgame_default_allocator);
	*blocker = (bgame_reload_blocker_t){
		.file = file,
		.line = line,
	};
	bgame_handle_t handle = bgame_handle_map_alloc(&bgame_reload_blockers, blocker);

	++bgame_reload_block_counter;

	blog_write(
		BLOG_LEVEL_DEBUG, file, line,
		"<-- Reload block added (num blockers: %d)", bgame_reload_block_counter
	);

	return (bgame_reload_block_t){ handle };
}

void
bgame_unblock_reload(bgame_reload_block_t block) {
	bgame_reload_blocker_t* blocker = bgame_handle_map_free(&bgame_reload_blockers, block.internal);
	if (blocker == NULL) { return; }

	--bgame_reload_block_counter;

	blog_write(
		BLOG_LEVEL_DEBUG, blocker->file, blocker->line,
		"<-- Reload block removed (num blockers: %d)", bgame_reload_block_counter
	);

	bgame_free(blocker, bgame_default_allocator);
}

bool
bgame_is_reload_enabled(void) {
	return bgame_reload_block_counter == 0;
}

void
bgame_remodule(bgame_app_t app, remodule_op_t op, void* userdata) {
	bgame_loader_interface_t* loader_interface = bgame_loader_interface = userdata;

	switch (op) {
		case REMODULE_OP_LOAD:
			bgame_on_load();

			loader_interface->app = app;
			loader_interface->update = bgame_update;
			loader_interface->is_reload_blocked = bgame_is_reload_blocked;
			loader_interface->explain_reload_blocked = bgame_explain_reload_blocked;

			loader_interface->bsfn = bsfn_ctx_create(bgame_default_allocator);
			bsfn_bind(loader_interface->bsfn);

			bgame_handle_map_init(&bgame_reload_blockers, bgame_default_allocator);

			BLOG_INFO("App loaded");
			break;
		case REMODULE_OP_UNLOAD:
			BLOG_INFO("Unloading app");

			bgame_handle_map_cleanup(&bgame_reload_blockers);

			bsfn_unbind(loader_interface->bsfn);
			bsfn_ctx_destroy(loader_interface->bsfn);

			bgame_on_unload();
			break;
		case REMODULE_OP_BEFORE_RELOAD:
			BLOG_INFO("Reloading app");

			bgame_before_reload();
			if (app.before_reload != NULL) {
				app.before_reload();
			}

			bgame_handle_map_cleanup(&bgame_reload_blockers);
			break;
		case REMODULE_OP_AFTER_RELOAD:
			bgame_after_reload();

			loader_interface->app = app;
			loader_interface->update = bgame_update;
			loader_interface->is_reload_blocked = bgame_is_reload_blocked;
			loader_interface->explain_reload_blocked = bgame_explain_reload_blocked;

			bsfn_bind(loader_interface->bsfn);

			bgame_handle_map_init(&bgame_reload_blockers, bgame_default_allocator);

			if (app.after_reload != NULL) {
				app.after_reload();
			}
			BLOG_INFO("App reloaded");

			BLOG_INFO("Reinitializing");
			app.init(loader_interface->argc, loader_interface->argv);
			BLOG_INFO("Reinitialized");
			break;
	}
}

#else

#include <cute_app.h>

int
bgame_static(bgame_app_t app, int argc, const char** argv) {
	bgame_on_load();

	app.init(argc, argv);
	while (cf_app_is_running()) {
		bgame_frame_allocator_next_frame();
		app.update();
	}
	app.cleanup();

	bgame_on_unload();
	return 0;
}

bgame_reload_block_t
bgame_block_reload_at(const char* file, int line) {
}

void
bgame_unblock_reload(bgame_reload_block_t block) {
}

bool
bgame_is_reload_enabled(void) {
	return false;
}

#endif

#include "internal.h"
#include <bgame/reloadable.h>
#include <blog.h>
#include "loader_interface.h"

extern void
bgame_frame_allocator_next_frame(void);

#if BGAME_RELOADABLE

static int bgame_reload_block_counter = 0;
static bgame_loader_interface_t* bgame_loader_interface = NULL;

static void
bgame_update(bgame_loader_interface_t* interface) {
	bgame_frame_allocator_next_frame();
	interface->app.update();
}

void
bgame_block_reload(void) {
	++bgame_reload_block_counter;
	bgame_loader_interface->reload_blocked = true;
}

void
bgame_unblock_reload(void) {
	--bgame_reload_block_counter;
	if (bgame_reload_block_counter < 0) {
		bgame_reload_block_counter = 0;
	}
	bgame_loader_interface->reload_blocked = bgame_reload_block_counter != 0;
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
			BLOG_INFO("App loaded");
			break;
		case REMODULE_OP_UNLOAD:
			BLOG_INFO("Unloading app");
			bgame_on_unload();
			break;
		case REMODULE_OP_BEFORE_RELOAD:
			BLOG_INFO("Reloading app");
			bgame_before_reload();
			if (app.before_reload != NULL) {
				app.before_reload();
			}
			break;
		case REMODULE_OP_AFTER_RELOAD:
			bgame_after_reload();

			BLOG_INFO("App reloaded");
			loader_interface->app = app;
			loader_interface->update = bgame_update;
			if (app.after_reload != NULL) {
				app.after_reload();
			}
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

void
bgame_block_reload(void) {
}

void
bgame_unblock_reload(void) {
}

bool
bgame_is_reload_enabled(void) {
	return false;
}

#endif

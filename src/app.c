#include <bgame/app.h>
#include <bgame/reloadable.h>
#include <bgame/allocator.h>
#include <bgame/allocator/frame.h>
#include <bfmt_log.h>
#include <cute_app.h>
#include <cute_file_system.h>
#include <cute_result.h>
#include <SDL3/SDL_video.h>
#include <dcimgui.h>
#include <stdlib.h>
#include <string.h>

static const bgame_app_t bgame_no_app = { 0 };

const bgame_app_t* bgame_entry_app = &bgame_no_app;

BGAME_PRIVATE_VAR(bgame_app, char*, bgame_imgui_ini_path)

const bgame_app_t*
bgame_app_info(void) {
	return bgame_entry_app;
}

static const char*
bgame_app_user_dir(void) {
	const bgame_app_t* app = bgame_entry_app;
	if (app->company == NULL || app->name == NULL) {
		BFMT_ERROR("The app has no company or no name: no user directory");
		return NULL;
	}

	const char* user_dir = cf_fs_get_user_directory(app->company, app->name);
	if (user_dir == NULL) {
		BFMT_ERROR("No user directory for ", app->company, "/", app->name);
	}
	return user_dir;
}

void
bgame_app_make(int width, int height, int options, const char* argv0) {
	const bgame_app_t* app = bgame_entry_app;
	const char* title = app->title != NULL ? app->title : app->name;
	if (title == NULL) { title = ""; }

	BFMT_INFO("Creating app");
	options |= CF_APP_OPTIONS_FILE_SYSTEM_DONT_DEFAULT_MOUNT_BIT;
	CF_Result result = cf_make_app(title, 0, 0, 0, width, height, options, argv0);
	if (result.code != CF_RESULT_SUCCESS) {
		BFMT_FATAL("Could not create app: ", result.details);
		abort();
	}

#if BGAME_RELOADABLE
	SDL_Window* window = cf_app_get_window();
	if (window != NULL) {
		SDL_SetWindowAlwaysOnTop(window, true);
	}
#endif
}

// A packed build ships its assets as an archive beside the executable; a
// development run reads the directory from the project root.
bool
bgame_app_mount_assets(void) {
	const char* base_dir = cf_fs_get_base_directory();
	if (base_dir != NULL) {
		bgame_str_t archive = bgame_print(base_dir, "assets.bin");
		if (cf_fs_mount(archive.chars, "/assets", true).code == CF_RESULT_SUCCESS) {
			BFMT_INFO("Mounted ", archive.chars);
			return true;
		}
	}

	CF_Result result = cf_fs_mount("./assets", "/assets", true);
	if (result.code != CF_RESULT_SUCCESS) {
		BFMT_ERROR("Could not mount assets: ", result.details);
		return false;
	}

	BFMT_INFO("Mounted ./assets");
	return true;
}

const char*
bgame_app_mount_user_dir(void) {
	const char* user_dir = bgame_app_user_dir();
	if (user_dir == NULL) {
		BFMT_ERROR("Nothing can be saved");
		return NULL;
	}

	CF_Result result = cf_fs_set_write_directory(user_dir);
	if (result.code != CF_RESULT_SUCCESS) {
		BFMT_ERROR("Could not set the write directory ", user_dir, ": ", result.details);
	}

	result = cf_fs_mount(user_dir, "/", true);
	if (result.code != CF_RESULT_SUCCESS) {
		BFMT_ERROR("Could not mount ", user_dir, ": ", result.details);
	}

	BFMT_INFO("User directory: ", user_dir);
	return user_dir;
}

void
bgame_app_init_imgui(void) {
	cf_app_init_imgui();

	bgame_free(bgame_imgui_ini_path, bgame_default_allocator);
	bgame_imgui_ini_path = NULL;

	const char* user_dir = bgame_app_user_dir();
	if (user_dir != NULL) {
		bgame_str_t path = bgame_print(user_dir, "imgui.ini");
		bgame_imgui_ini_path = bgame_malloc((size_t)path.len + 1, bgame_default_allocator);
		memcpy(bgame_imgui_ini_path, path.chars, (size_t)path.len + 1);
	}
	ImGui_GetIO()->IniFilename = bgame_imgui_ini_path;
}

void
bgame_app_destroy(void) {
	cf_destroy_app();

	bgame_free(bgame_imgui_ini_path, bgame_default_allocator);
	bgame_imgui_ini_path = NULL;
}

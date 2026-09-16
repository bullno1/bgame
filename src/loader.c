#if defined(__linux__) && !defined(_GNU_SOURCE)
#	define _GNU_SOURCE 1
#endif

#define REMODULE_HOST_IMPLEMENTATION
#include <bgame/reloadable.h>

#if BGAME_RELOADABLE

#include <string.h>
#include <stdio.h>
#include <bresmon.h>
#include <cute_app.h>
#include <SDL3/SDL_init.h>
#include "loader_interface.h"
#define BRESMON_IMPLEMENTATION
#include <bresmon.h>

#ifdef _WIN32
    #define PATH_SEPARATOR '\\'
#else
    #define PATH_SEPARATOR '/'
#endif

#if defined(_WIN32)
#	define DYNLIB_EXT ".dll"
#elif defined(__APPLE__)
#	define DYNLIB_EXT ".dylib"
#elif defined(__linux__)
#	define DYNLIB_EXT ".so"
#endif

static void
reload_module(const char* path, void* module) {
	(void)path;
	remodule_reload(module);
}

static bool
is_reload_blocked(bgame_loader_interface_t* loader_interface) {
	return loader_interface->is_reload_blocked != NULL
		&& loader_interface->is_reload_blocked(loader_interface);
}

static struct {
	bgame_loader_interface_t interface;
	remodule_t* module;
	bresmon_t* monitor;
	bool reload_needed;
} bgame_loader = { 0 };

SDL_AppResult SDLCALL
SDL_AppInit(void** appstate, int argc, char* argv[]) {
	(void)appstate;

    const char* last_separator = strrchr(argv[0], PATH_SEPARATOR);
    const char* exe_name = last_separator != NULL ? last_separator + 1 : argv[0];
	const char* dot = strrchr(exe_name, '.');
	size_t basename_len = dot != NULL ? dot - exe_name: strlen(exe_name);

	char module_name[128];
	if (basename_len + sizeof(DYNLIB_EXT) > sizeof(module_name)) {
		fprintf(stderr, "Executable name is too long: %s\n", exe_name);
		return SDL_APP_FAILURE;
	}

	memcpy(module_name, exe_name, basename_len);
	memcpy(&module_name[basename_len], DYNLIB_EXT, sizeof(DYNLIB_EXT));

	bgame_loader.interface = (bgame_loader_interface_t){
		.argc = argc,
		.argv = (const char**)argv,
	};

	bgame_loader.module = remodule_load(module_name, &bgame_loader.interface);
	bgame_loader.monitor = bresmon_create(NULL);
	bresmon_watch(bgame_loader.monitor, remodule_path(bgame_loader.module), reload_module, bgame_loader.module);

	bgame_loader.interface.app.init(argc, (const char**)argv);

	return SDL_APP_CONTINUE;
}

SDL_AppResult SDLCALL
SDL_AppIterate(void* appstate) {
	(void)appstate;
	bgame_loader_interface_t* loader_interface = &bgame_loader.interface;

	if (!cf_app_is_running()) { return SDL_APP_SUCCESS; }

	loader_interface->update(loader_interface);

	if (bresmon_should_reload(bgame_loader.monitor, false)) {
		bgame_loader.reload_needed = true;

		if (
			is_reload_blocked(loader_interface)
			&&
			loader_interface->explain_reload_blocked != NULL
		) {
			loader_interface->explain_reload_blocked(loader_interface);
		}
	}

	if (bgame_loader.reload_needed && !is_reload_blocked(loader_interface)) {
		bresmon_reload(bgame_loader.monitor);
		bgame_loader.reload_needed = false;
	}

	return cf_app_is_running() ? SDL_APP_CONTINUE : SDL_APP_SUCCESS;
}

void SDLCALL
SDL_AppQuit(void* appstate, SDL_AppResult result) {
	(void)appstate;
	(void)result;

	// SDL_AppInit failed before loading the module
	if (bgame_loader.module == NULL) { return; }

	bgame_loader.interface.app.cleanup();

	bresmon_destroy(bgame_loader.monitor);
	remodule_unload(bgame_loader.module);
}

#endif

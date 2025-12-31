#define REMODULE_HOST_IMPLEMENTATION
#include <bgame/reloadable.h>

#if BGAME_RELOADABLE

#include <string.h>
#include <stdio.h>
#include <bresmon.h>
#include <cute_app.h>
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

int
bgame_main(int argc, const char* argv[]) {
    const char* last_separator = strrchr(argv[0], PATH_SEPARATOR);
    const char* exe_name = last_separator != NULL ? last_separator + 1 : argv[0];
	char* dot = strrchr(exe_name, '.');
	size_t basename_len = dot != NULL ? dot - exe_name: strlen(exe_name);

	char module_name[128];
	if (basename_len + sizeof(DYNLIB_EXT) > sizeof(module_name)) {
		fprintf(stderr, "Executable name is too long: %s\n", exe_name);
		return 1;
	}

	memcpy(module_name, exe_name, basename_len);
	memcpy(&module_name[basename_len], DYNLIB_EXT, sizeof(DYNLIB_EXT));

	bgame_loader_interface_t loader_interface = {
		.argc = argc,
		.argv = argv,
		.reload_blocked = false,
	};

	remodule_t* module = remodule_load(module_name, &loader_interface);
	bresmon_t* monitor = bresmon_create(NULL);
	bresmon_watch(monitor, remodule_path(module), reload_module, module);

	loader_interface.app.init(argc, argv);

	bool reload_needed = false;
	while (cf_app_is_running()) {
		loader_interface.update(&loader_interface);

		if (bresmon_should_reload(monitor, false)) {
			reload_needed = true;
		}

		if (reload_needed && !loader_interface.reload_blocked) {
			bresmon_reload(monitor);
			reload_needed = false;
		}
	}

	loader_interface.app.cleanup();

	bresmon_destroy(monitor);
	remodule_unload(module);

	return 0;
}

#else

extern int bgame_entry(int argc, const char** argv);

int
bgame_main(int argc, const char** argv) {
	return bgame_entry(argc, argv);
}

#endif

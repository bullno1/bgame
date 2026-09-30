#include "internal.h"

AUTOLIST_DEFINE(bgame_on_load_handlers)
AUTOLIST_DEFINE(bgame_on_unload_handlers)
AUTOLIST_DEFINE(bgame_before_reload_handlers)
AUTOLIST_DEFINE(bgame_after_reload_handlers)

void
bgame_on_load(void) {
	// Handlers run in link order, which differs between toolchains
	bgame_allocator_init();
	bgame_log_init();

	AUTOLIST_FOREACH(entry, bgame_on_load_handlers) {
		bgame_lifecycle_fn_t fn = *(bgame_lifecycle_fn_t*)entry->value_addr;
		fn();
	}
}

void
bgame_on_unload(void) {
	AUTOLIST_FOREACH(entry, bgame_on_unload_handlers) {
		bgame_lifecycle_fn_t fn = *(bgame_lifecycle_fn_t*)entry->value_addr;
		fn();
	}
}


void
bgame_before_reload(void) {
	AUTOLIST_FOREACH(entry, bgame_before_reload_handlers) {
		bgame_lifecycle_fn_t fn = *(bgame_lifecycle_fn_t*)entry->value_addr;
		fn();
	}
}


void
bgame_after_reload(void) {
	bgame_allocator_init();
	bgame_log_init();

	AUTOLIST_FOREACH(entry, bgame_after_reload_handlers) {
		bgame_lifecycle_fn_t fn = *(bgame_lifecycle_fn_t*)entry->value_addr;
		fn();
	}
}

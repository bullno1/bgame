#ifndef BGAME_INTERNAL_H
#define BGAME_INTERNAL_H

#include <autolist.h>

typedef void (*bgame_lifecycle_fn_t)(void);

#define BGAME_ON_LOAD(NAME) BGAME_LIFECYCLE_FN(NAME, on_load)
#define BGAME_ON_UNLOAD(NAME) BGAME_LIFECYCLE_FN(NAME, on_unload)
#define BGAME_BEFORE_RELOAD(NAME) BGAME_LIFECYCLE_FN(NAME, before_reload)
#define BGAME_AFTER_RELOAD(NAME) BGAME_LIFECYCLE_FN(NAME, after_reload)

#define BGAME_LIFECYCLE_FN(NAME, EVENT) \
	static void NAME##_##EVENT(void); \
	AUTOLIST_ENTRY(bgame_##EVENT##_handlers, bgame_lifecycle_fn_t, NAME##_##EVENT##_handler) = NAME##_##EVENT; \
	static void NAME##_##EVENT(void)

void
bgame_on_load(void);

void
bgame_before_reload(void);

void
bgame_after_reload(void);

void
bgame_on_unload(void);

#endif

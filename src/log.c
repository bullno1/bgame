#include "internal.h"
#include <bgame/reloadable.h>
#define BLOG_IMPLEMENTATION
#include <blog.h>

static blog_file_logger_options_t bgame_log_options = {
	.with_colors = true,
};

static void
bgame_log_init(void) {
	blog_init(&(blog_options_t){
		.current_filename = __FILE__,
		.current_depth_in_project = 2,
	});

	bgame_log_options.file = stderr;
	blog_level_t level = BLOG_LEVEL_DEBUG;
#if BGAME_RELOADABLE
	level = BLOG_LEVEL_TRACE;
#endif
	blog_add_file_logger(level, &bgame_log_options);
}

BGAME_ON_LOAD(bgame_log) {
	bgame_log_init();
}

BGAME_AFTER_RELOAD(bgame_log) {
	bgame_log_init();
}

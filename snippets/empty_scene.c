#include <bgame/scene.h>
#include <bgame/reloadable.h>
#include <cute.h>

#define SCENE_NAME empty_scene
#define SCENE_VAR(TYPE, NAME) BGAME_PRIVATE_VAR(SCENE_NAME, TYPE, NAME)

static void
init(void) {
}

static void
cleanup(void) {
}

static void
update(void) {
	cf_app_update(NULL);

	cf_app_draw_onto_screen(true);
}

BGAME_SCENE(SCENE_NAME) = {
	.init = init,
	.update = update,
	.cleanup = cleanup,
};

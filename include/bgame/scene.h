#ifndef BGAME_SCENE_H
#define BGAME_SCENE_H

#include <autolist.h>
#include <bmacro.h>
#include <bgame/reloadable.h>
#include <stdbool.h>
#include <stddef.h>

typedef enum {
	BGAME_SCENE_INITIALIZING,
	BGAME_SCENE_REINITIALIZING,
	BGAME_SCENE_RUNNING,
	BGAME_SCENE_SUSPENDING,
	BGAME_SCENE_RESUMING,
	BGAME_SCENE_CLEANING_UP,
} bgame_scene_state_t;

typedef struct bgame_scene_s {
	void (*init)(void);
	void (*update)(void);
	// Runs on a running or a suspended scene alike
	void (*cleanup)(void);

	void (*suspend)(void);
	void (*resume)(void);

	void (*check_reload)(void);
	void (*before_reload)(void);
	void (*after_reload)(void);
} bgame_scene_def_t;

#define BGAME_SCENE(NAME) \
	AUTOLIST_ENTRY_EX(bgame_scene_list, bgame_scene_def_t, NAME, BCONCAT(bgame_scene_, NAME))

typedef struct bgame_scene_var_s {
	const char* scene;
	void* addr;
	size_t size;
} bgame_scene_var_t;

/**
 * Declare a scene-private variable.
 *
 * The variable persists across hot reloads but is zeroed when the scene exits
 * (switch or pop), right after its `cleanup` callback returns.
 */
#define BGAME_SCENE_VAR(SCENE, TYPE, NAME) \
	BGAME_PRIVATE_VAR(SCENE, TYPE, NAME) \
	static const bgame_scene_var_t BCONCAT(bgame__scene_var_, NAME) = { \
		.scene = BSTRINGIFY(SCENE), \
		.addr = &NAME, \
		.size = sizeof(NAME), \
	}; \
	AUTOLIST_ADD_ENTRY( \
		bgame_scene_var_list, \
		BCONCAT(SCENE, BCONCAT(_, NAME)), \
		BCONCAT(bgame__scene_var_, NAME) \
	)

typedef struct bgame_scene_reg_s {
	const char* name;
	const bgame_scene_def_t* def;
} bgame_scene_reg_t;

/**
 * Declare a scene for other units.
 *
 * The optional type is the scene's argument, declared as `scene_<NAME>_arg_t`.
 * A scene without one takes no argument.
 */
#define BGAME_DECLARE_SCENE(NAME, ...) \
	extern bgame_scene_def_t BCONCAT(bgame_scene_, NAME); \
	__VA_OPT__(typedef __VA_ARGS__ BCONCAT(scene_, BCONCAT(NAME, _arg_t));)

#ifndef BGAME_SCENE_ARG_MAX
#	define BGAME_SCENE_ARG_MAX 256
#endif

typedef struct bgame_scene_arg_s {
	const void* data;
	size_t size;
} bgame_scene_arg_t;

#define BGAME__SCENE_ARG_SIZE(T) \
	(sizeof(T) + 0 * sizeof(char[sizeof(T) <= BGAME_SCENE_ARG_MAX ? 1 : -1]))

#define BGAME__SCENE_ARG(T, ...) \
	(bgame_scene_arg_t){ __VA_OPT__( \
		.data = (T[]){ [0] = __VA_ARGS__ }, \
		.size = BGAME__SCENE_ARG_SIZE(T), \
	) }

/**
 * Switch to a scene, replacing the current one.
 *
 * A scene runs at most once in the stack: when it is already lower in the
 * stack, everything above it and the scene itself are cleaned up, and it
 * starts anew in its slot.
 *
 * The last argument, if any, is either a brace initializer or a value of
 * `scene_<NAME>_arg_t`. It is copied and stays readable through
 * @ref bgame_scene_arg for the scene's whole life, zeroed when none is given.
 *
 * @param NAME the scene, declared with @ref BGAME_DECLARE_SCENE
 * @param ... the argument
 */
#define bgame_switch_scene(NAME, ...) \
	bgame__switch_scene( \
		&BCONCAT(bgame_scene_, NAME), \
		BGAME__SCENE_ARG(BCONCAT(scene_, BCONCAT(NAME, _arg_t)) __VA_OPT__(, __VA_ARGS__)) \
	)

/**
 * Suspend the current scene and run another on top of it.
 *
 * Refused with an error for a scene already in the stack; switch to it instead.
 * The argument follows @ref bgame_switch_scene.
 */
#define bgame_push_scene(NAME, ...) \
	bgame__push_scene( \
		&BCONCAT(bgame_scene_, NAME), \
		BGAME__SCENE_ARG(BCONCAT(scene_, BCONCAT(NAME, _arg_t)) __VA_OPT__(, __VA_ARGS__)) \
	)

/**
 * The argument the current scene was switched to or pushed with.
 *
 * Reads as zero when the scene was started without one, or when the current
 * scene is not @p NAME, which is an error.
 */
#define bgame_scene_arg(NAME) \
	((const BCONCAT(scene_, BCONCAT(NAME, _arg_t))*)bgame__scene_arg( \
		&BCONCAT(bgame_scene_, NAME), \
		BGAME__SCENE_ARG_SIZE(BCONCAT(scene_, BCONCAT(NAME, _arg_t))) \
	))

void
bgame__switch_scene(const bgame_scene_def_t* def, bgame_scene_arg_t arg);

void
bgame__push_scene(const bgame_scene_def_t* def, bgame_scene_arg_t arg);

const void*
bgame__scene_arg(const bgame_scene_def_t* def, size_t size);

/**
 * Switch to a scene by name, without an argument.
 *
 * For a name that comes from outside the code; use @ref bgame_switch_scene
 * otherwise.
 */
void
bgame_switch_scene_by_name(const char* name);

/**
 * Restart the current scene with the argument it was started with.
 */
void
bgame_reload_scene(void);

/**
 * Push a scene by name, without an argument.
 *
 * @see bgame_switch_scene_by_name
 */
void
bgame_push_scene_by_name(const char* name);

void
bgame_pop_scene(void);

void
bgame_clear_scene_stack(void);

bgame_scene_reg_t
bgame_current_scene(void);

const char*
bgame_current_scene_name(void);

bgame_scene_state_t
bgame_current_scene_state(void);

int
bgame_scene_stack_depth(void);

void
bgame_scene_update(void);

void
bgame_scene_check_reload(void);

void
bgame_scene_before_reload(void);

void
bgame_scene_after_reload(void);

void
bgame_list_scenes(bgame_scene_reg_t** scenes, int* num_scenes);

#endif

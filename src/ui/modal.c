#include <bgame/ui/modal.h>
#include <bgame/ui/presence.h>
#include <bgame/ui/focus.h>
#include <bgame/reloadable.h>
#include <bfmt_log.h>

typedef struct {
	bgame_ui_id_t current;
	bool closing;
} modal_state_t;

BGAME_VAR(modal_state_t, bgame_ui_modal_state) = { 0 };

// The modal whose block is running
static BUI_CTX_TAG(bgame_ui_id_t) modal_ctx;

static bgame_ui_id_t
modal_id(const char* name) {
	return bgame_ui_make_id(bgame_ui_make_id(0, "bgame_ui_modal", 0), name, 0);
}

bool
bgame_ui_modal_open(const char* name) {
	modal_state_t* state = &bgame_ui_modal_state;
	if (state->current != 0) { return false; }
	state->current = modal_id(name);
	state->closing = false;
	return true;
}

void
bgame_ui_modal_close(void) {
	modal_state_t* state = &bgame_ui_modal_state;
	bgame_ui_id_t id = *bgame_ui_peek_ctx(modal_ctx);
	if (id == 0) { id = state->current; }
	if (id == 0) {
		BFMT_ERROR("No modal to close");
		return;
	}
	if (id != state->current) {
		BFMT_ERROR("Closing a modal that is not the open one");
		return;
	}
	state->closing = true;
}

bool
bgame_ui_modal_begin_at(bgame_ui_loc_t loc, const char* name, const bgame_ui_element_t* element) {
	modal_state_t* state = &bgame_ui_modal_state;
	bgame_ui_id_t id = modal_id(name);
	if (state->current != id) { return false; }

	bgame_ui_element_t elem = *element;
	elem.floating.attach = BGAME_UI_ATTACH_ROOT;
	elem.focus.kind = BGAME_UI_FOCUS_GROUP;
	elem.focus.flags |= BGAME_UI_FOCUS_MODAL | BGAME_UI_FOCUS_AUTOFOCUS;

	bgame_ui_key_t key = bgame_ui_make_key(0, name, 0);
	key.id = id;
	if (!bgame_ui_presence_begin_at(loc, key, !state->closing, &elem)) {
		// The exit has run
		state->current = 0;
		state->closing = false;
		return false;
	}
	*bgame_ui_push_ctx(modal_ctx) = id;
	return true;
}

void
bgame_ui_modal_end(void) {
	bgame_ui_pop_ctx(modal_ctx);
	bgame_ui_presence_end();
}

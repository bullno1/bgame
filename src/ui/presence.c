#include <bgame/ui/presence.h>
#include <bfmt_log.h>

bool
bgame_ui_presence_begin_at(bgame_ui_loc_t loc, bgame_ui_key_t key, bool present, const bgame_ui_element_t* element) {
	if (key.id == 0 && key.name == NULL) {
		BFMT_ERROR("An anonymous element cannot be a presence");
		return false;
	}
	bgame_ui_id_t id = key.id != 0 ? key.id : bgame_ui_make_id(bgame_ui_scope_id(), key.name, key.index);

	// Gone when never there, when it has no transition, or when its exit has run
	if (!present && (bgame_ui_transition_state(id) == NULL || bgame_ui_exited(id))) {
		return false;
	}

	bgame_ui_element_t elem = *element;
	elem.transition.exiting = !present;
	bgame_ui__begin(loc, key);
	bgame_ui__configure(&elem);
	bgame_ui__block();
	return true;
}

void
bgame_ui_presence_end(void) {
	bgame_ui_close();
}

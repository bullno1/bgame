#ifndef BGAME_UI_INSPECT_H
#define BGAME_UI_INSPECT_H

#include "../ui.h"

typedef enum {
	/*! Stop at the next left press, with what is under the pointer selected */
	BGAME_UI_PICK_LOCK_ON_CLICK = 1 << 0,
	/*! Stop at the next right press, with the selection from before the pick */
	BGAME_UI_PICK_CANCEL_ON_ALT = 1 << 1,
	/*! Skip anonymous elements */
	BGAME_UI_PICK_NAMED         = 1 << 2,
	/*! Let the UI see the pointer while picking. */
	BGAME_UI_PICK_PASS_POINTER  = 1 << 3,
	/**
	 * On lock, open the picked element's declaration with the editor command
	 *
	 * The command is built from the `BGAME_EDITOR` environment variable, processed
	 * as follow:
	 *
	 * * The string is split by white space into separate arguments
	 * * Double quotes group tokens into a single argument
	 * * `{file}` and `{line}` are substituted with the actual source location
	 *
	 * It is then executed without a shell.
	 * Example templates for popular editors:
	 *
	 *   * vim: `vim --servername GAME --remote-silent "+{line}|set cursorline|normal! $" {file}`
	 *     for a vim instance started with `--servername GAME`
	 *   * nvim: `nvim --server /tmp/nvim.sock --remote-send "<C-\><C-N>:silent drop {file} | {line}<CR>"`
	 *     for a nvim instance started with `--listen /tmp/nvim.sock`
	 *   * emacs: `emacsclient -n +{line} {file}`
	 *   * vscode: `code --goto {file}:{line}`
	 */
	BGAME_UI_PICK_EDIT          = 1 << 4,
} bgame_ui_pick_flag_t;

/*! Select what is under the pointer, every frame, until stopped or a flag ends it. The selection is highlighted meanwhile. */
void
bgame_ui_inspect_pick_start(uint32_t flags);

void
bgame_ui_inspect_pick_stop(void);

bool
bgame_ui_inspect_picking(void);

/*! The element a pick locked on this frame, 0 otherwise */
bgame_ui_id_t
bgame_ui_inspect_picked(void);

/*! The inspector's selection, 0 for none */
bgame_ui_id_t
bgame_ui_inspect_selected(void);

void
bgame_ui_inspect_select(bgame_ui_id_t id);

/**
 * Open an element's declaration with the editor command, false when the element is unknown
 *
 * The location is logged; the command in the `BGAME_EDITOR` environment variable, if
 * set, runs with `{file}` and `{line}` replaced. See @ref BGAME_UI_PICK_EDIT for templates.
 */
bool
bgame_ui_inspect_edit(bgame_ui_id_t id);

/*! Draw the tree and the selection's properties with dear imgui, for last frame's layout */
void
bgame_ui_inspect(void);

#endif

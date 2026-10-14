#ifndef BGAME_ASSET_TRANSLATION_H
#define BGAME_ASSET_TRANSLATION_H

#include <bgame/asset.h>

/**
 * The translations of one language: a gettext `.po` file.
 *
 * Entries are keyed by context and source text.
 */
typedef struct bgame_translation_s bgame_translation_t;

bgame_translation_t*
bgame_load_translation(bgame_asset_bundle_t* bundle, const char* path);

/*! The `Language:` header, empty without one */
const char*
bgame_translation_language(const bgame_translation_t* translation);

/*! The translation of a text, or NULL for none; the context may be NULL */
const char*
bgame_translation_lookup(const bgame_translation_t* translation, const char* context, const char* key);

/*! Forget a translation, so that the source text is used from now on */
void
bgame_translation_drop(bgame_translation_t* translation, const char* context, const char* key);

extern bgame_asset_type_t translation;

#define BGAME_TRANSLATION(NAME, ...) BGAME_ASSET(NAME, translation, bgame_translation_t*, __VA_ARGS__)

#endif

#include <bgame/asset/9patch.h>
#include <bgame/asset.h>
#include <blog.h>
#include <cute_image.h>
#include <cute_sprite.h>

struct bgame_9patch_s {
	bgame_9patch_config_t config;
	int center_width;
	int center_height;

	// Sprite for each patch named after the direction + (c)enter
	struct CF_Sprite nw, n, ne;
	struct CF_Sprite  w, c,  e;
	struct CF_Sprite sw, s, se;
};

static inline void
bgame_9patch_init_patch(
	CF_Sprite* sprite,
	CF_Pixel* tmp_buf,
	CF_Image src,
	int x, int y,
	int w, int h
) {
	for (int iy = 0; iy < h; ++iy) {
		for (int ix = 0; ix < w; ++ix) {
			tmp_buf[ix + iy * w] = src.pix[(x + ix) + (y + iy) * src.w];
		}
	}

	if (sprite->name == NULL) {
		CF_Sprite patch = cf_make_easy_sprite_from_pixels(tmp_buf, w, h);
		patch.offset.x = w * 0.5;
		patch.offset.y = -h * 0.5;
		*sprite = patch;
	} else {
		cf_easy_sprite_update_pixels(sprite, tmp_buf);
	}
}

static void
bgame_9patch_unload(
	bgame_asset_bundle_t* bundle,
	void* asset
) {
	bgame_9patch_t* nine_patch = asset;
	if (nine_patch->nw.name != NULL) {
		cf_easy_sprite_unload(&nine_patch->nw);
		cf_easy_sprite_unload(&nine_patch->n);
		cf_easy_sprite_unload(&nine_patch->ne);

		cf_easy_sprite_unload(&nine_patch->w);
		cf_easy_sprite_unload(&nine_patch->c);
		cf_easy_sprite_unload(&nine_patch->e);

		cf_easy_sprite_unload(&nine_patch->sw);
		cf_easy_sprite_unload(&nine_patch->s);
		cf_easy_sprite_unload(&nine_patch->se);

		nine_patch->nw = cf_sprite_defaults();
		nine_patch->n  = cf_sprite_defaults();
		nine_patch->ne = cf_sprite_defaults();

		nine_patch->w  = cf_sprite_defaults();
		nine_patch->c  = cf_sprite_defaults();
		nine_patch->e  = cf_sprite_defaults();

		nine_patch->sw = cf_sprite_defaults();
		nine_patch->s  = cf_sprite_defaults();
		nine_patch->se = cf_sprite_defaults();
	}
}

static bgame_asset_load_result_t
bgame_9patch_load(
	bgame_asset_bundle_t* bundle,
	void* asset,
	const char* path,
	const void* args
) {
	bgame_9patch_t* nine_patch = asset;
	bgame_9patch_config_t config = args != NULL
		? *(bgame_9patch_config_t*)args
		: nine_patch->config;

	if (
		!bgame_asset_source_changed(bundle, nine_patch)
		&& (
			config.left == nine_patch->config.left
			&& config.right == nine_patch->config.right
			&& config.top == nine_patch->config.top
			&& config.bottom == nine_patch->config.bottom
		)
	) {
		return BGAME_ASSET_UNCHANGED;
	}

	CF_Image src;
	CF_Result result = cf_image_load_png(path, &src);
	cf_image_premultiply(&src);
	if (result.code != CF_RESULT_SUCCESS) {
		BLOG_ERROR("Could not load image: %s", path);
		return BGAME_ASSET_ERROR;
	}

	bool identical_config = true
		&& nine_patch->config.left == config.left
		&& nine_patch->config.right == config.right
		&& nine_patch->config.top == config.top
		&& nine_patch->config.bottom == config.bottom
		&& src.w == nine_patch->center_width + config.left + config.right
		&& src.h == nine_patch->center_height + config.top + config.bottom;

	// If it's already loaded with a different config
	if (nine_patch->nw.name != NULL && !identical_config) {
		bgame_9patch_unload(bundle, nine_patch);
	}

	// Border size for the patches
	int p_left   = config.left;
	int p_top    = config.top;
	int p_right  = config.right;
	int p_bottom = config.bottom;
	// Width and height of the center piece
	int p_c_w = src.w - p_left - p_right;  // patch's center width
	int p_c_h = src.h - p_top - p_bottom;  // patch's center height

	CF_Pixel* img_buf = bgame_asset_malloc(bundle, src.w * src.h * sizeof(CF_Pixel));

	bgame_9patch_init_patch(&nine_patch->nw, img_buf, src, 0              , 0               , p_left , p_top);
	bgame_9patch_init_patch(&nine_patch->n , img_buf, src, p_left         , 0               , p_c_w  , p_top);
	bgame_9patch_init_patch(&nine_patch->ne, img_buf, src, src.w - p_right, 0               , p_right, p_top);

	bgame_9patch_init_patch(&nine_patch->w , img_buf, src, 0              , p_top           , p_left , p_c_h);
	bgame_9patch_init_patch(&nine_patch->c , img_buf, src, p_left         , p_top           , p_c_w  , p_c_h);
	bgame_9patch_init_patch(&nine_patch->e , img_buf, src, src.w - p_right, p_top           , p_right, p_c_h);

	bgame_9patch_init_patch(&nine_patch->sw, img_buf, src, 0              , src.h - p_bottom, p_left , p_bottom);
	bgame_9patch_init_patch(&nine_patch->s , img_buf, src, p_left         , src.h - p_bottom, p_c_w  , p_bottom);
	bgame_9patch_init_patch(&nine_patch->se, img_buf, src, src.w - p_right, src.h - p_bottom, p_right, p_bottom);

	nine_patch->config = config;
	nine_patch->center_width = p_c_w;
	nine_patch->center_height = p_c_h;

	bgame_asset_free(bundle, img_buf);
	cf_image_free(&src);

	return BGAME_ASSET_LOADED;
}

BGAME_ASSET_TYPE(nine_patch) = {
	.name = "9patch",
	.size = sizeof(bgame_9patch_t),
	.load = bgame_9patch_load,
	.unload = bgame_9patch_unload,
};

bgame_9patch_t*
bgame_load_9patch(
	struct bgame_asset_bundle_s* bundle,
	const char* path,
	bgame_9patch_config_t config
) {
	return bgame_asset_load(bundle, &nine_patch, path, &config);
}

static inline void
bgame_9patch_draw_patch(
	const CF_Sprite* patch,
	float x, float y,
	float x_scale, float y_scale
) {
	CF_Sprite sprite = *patch;
	sprite.transform.p.x = x;
	sprite.transform.p.y = y;
	sprite.scale.x = x_scale;
	sprite.scale.y = y_scale;
	cf_draw_sprite(&sprite);
}

void
bgame_draw_9patch(const bgame_9patch_t* nine_patch, CF_Aabb aabb) {
	if (nine_patch->nw.name == NULL) { return; }

	float p_left   = (float)nine_patch->config.left;
	float p_top    = (float)nine_patch->config.top;
	float p_right  = (float)nine_patch->config.right;
	float p_bottom = (float)nine_patch->config.bottom;

	float dst_width  = aabb.max.x - aabb.min.x;
	float dst_height = aabb.max.y - aabb.min.y;
	float x_scale = (dst_width - p_left - p_right) / (float)nine_patch->center_width;
	float y_scale = (dst_height - p_top - p_bottom) / (float)nine_patch->center_height;

	bgame_9patch_draw_patch(&nine_patch->nw, aabb.min.x          , aabb.max.y           , 1.f    , 1.f);
	bgame_9patch_draw_patch(&nine_patch->n , aabb.min.x + p_left , aabb.max.y           , x_scale, 1.f);
	bgame_9patch_draw_patch(&nine_patch->ne, aabb.max.x - p_right, aabb.max.y           , 1.f    , 1.f);

	bgame_9patch_draw_patch(&nine_patch->w , aabb.min.x          , aabb.max.y - p_top   , 1.f    , y_scale);
	bgame_9patch_draw_patch(&nine_patch->c , aabb.min.x + p_left , aabb.max.y - p_top   , x_scale, y_scale);
	bgame_9patch_draw_patch(&nine_patch->e , aabb.max.x - p_right, aabb.max.y - p_top   , 1.f    , y_scale);

	bgame_9patch_draw_patch(&nine_patch->sw, aabb.min.x          , aabb.min.y + p_bottom, 1.f    , 1.f);
	bgame_9patch_draw_patch(&nine_patch->s , aabb.min.x + p_left , aabb.min.y + p_bottom, x_scale, 1.f);
	bgame_9patch_draw_patch(&nine_patch->se, aabb.max.x - p_right, aabb.min.y + p_bottom, 1.f    , 1.f);
}

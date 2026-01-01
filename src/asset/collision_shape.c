#include <bgame/asset.h>
#include <bgame/asset/collision_shape.h>
#include <blog.h>
#include <cute.h>

static bgame_asset_load_result_t
bgame_collision_shape_load(
	bgame_asset_bundle_t* bundle,
	void* asset,
	const char* path,
	const void* args
) {
	bgame_collision_shape_t* shape = asset;

	if (!bgame_asset_source_changed(bundle, asset)) {
		return BGAME_ASSET_UNCHANGED;
	}

	bgame_asset_load_result_t result = BGAME_ASSET_ERROR;
	CF_JDoc jdoc = cf_make_json_from_file(path);
	CF_JVal root = cf_json_get_root(jdoc);

	const char* type = cf_json_get_string(cf_json_get(root, "type"));

	if (type == NULL) {
		BLOG_ERROR("Invalid shape file");
		goto end;
	}

	if (strcmp(type, "polygon") == 0) {
		shape->type = CF_SHAPE_TYPE_POLY;
		CF_JVal jvertices = cf_json_get(root, "vertices");

		int num_verts = cf_json_get_len(jvertices);
		if (num_verts > CF_POLY_MAX_VERTS) {
			BLOG_ERROR("Too many polgyons");
			goto end;
		}

		for (int i = 0; i < num_verts; ++i) {
			CF_JVal jvertex = cf_json_array_get(jvertices, i);
			shape->data.poly.verts[i].x = cf_json_get_float(cf_json_array_get(jvertex, 0));
			shape->data.poly.verts[i].y = cf_json_get_float(cf_json_array_get(jvertex, 1));
		}
		shape->data.poly.count = num_verts;
		cf_make_poly(&shape->data.poly);
		result = BGAME_ASSET_LOADED;
	} else {
		BLOG_ERROR("Invalid shape type");
		goto end;
	}

end:
	cf_destroy_json(jdoc);

	return result;
}

static void
bgame_collision_shape_unload(
	bgame_asset_bundle_t* bundle,
	void* asset
) {
}

BGAME_ASSET_TYPE(collision_shape) = {
	.name = "collision_shape",
	.size = sizeof(bgame_collision_shape_t),
	.load = bgame_collision_shape_load,
	.unload = bgame_collision_shape_unload,
};


bgame_collision_shape_t*
bgame_load_collision_shape(struct bgame_asset_bundle_s* bundle, const char* path) {
	return bgame_asset_load(bundle, &collision_shape, path, NULL);
}

#include <bgame/collision.h>

CF_Aabb
bgame_make_aabb_from_shape(const bgame_collision_shape_t* shape, CF_M3x2 transform) {
	switch (shape->type) {
		case CF_SHAPE_TYPE_NONE:
			return (CF_Aabb){ 0 };
		case CF_SHAPE_TYPE_AABB: {
			CF_V2 corners[4];
			cf_aabb_verts(corners, shape->data.aabb);

			// Transform each corner
			for (int i = 0; i < 4; i++) {
				corners[i] = cf_mul(transform, corners[i]);
			}
			return cf_make_aabb_verts(corners, CF_ARRAY_SIZE(corners));
		} break;
		case CF_SHAPE_TYPE_POLY: {
			CF_V2 verts[CF_POLY_MAX_VERTS];

			for (int i = 0; i < shape->data.poly.count; i++) {
				verts[i] = cf_mul(transform, shape->data.poly.verts[i]);
			}
			return cf_make_aabb_verts(verts, shape->data.poly.count);
		} break;
		case CF_SHAPE_TYPE_CIRCLE: {
			// Transform the center of the circle
			CF_V2 center_transformed = cf_mul(transform, shape->data.circle.p);

			// The radius gets scaled by the matrix
			// Get the scale from the matrix by transforming a unit vector
			CF_V2 scale_x = cf_sub(cf_mul(transform, cf_v2(1, 0)), cf_mul(transform, cf_v2(0, 0)));
			CF_V2 scale_y = cf_sub(cf_mul(transform, cf_v2(0, 1)), cf_mul(transform, cf_v2(0, 0)));
			float scale = cf_max(cf_len(scale_x), cf_len(scale_y));
			float transformed_radius = shape->data.capsule.r * scale;

			// Create AABB centered on the transformed circle center
			float min_x = center_transformed.x - transformed_radius;
			float max_x = center_transformed.x + transformed_radius;
			float min_y = center_transformed.y - transformed_radius;
			float max_y = center_transformed.y + transformed_radius;

			return cf_make_aabb(cf_v2(min_x, min_y), cf_v2(max_x, max_y));
		} break;
		case CF_SHAPE_TYPE_CAPSULE: {
			// Transform the two endpoints of the capsule
			CF_V2 a_transformed = cf_mul(transform, shape->data.capsule.a);
			CF_V2 b_transformed = cf_mul(transform, shape->data.capsule.b);

			// The radius also gets scaled by the matrix
			// Get the scale from the matrix by transforming a unit vector
			CF_V2 scale_x = cf_sub(cf_mul(transform, cf_v2(1, 0)), cf_mul(transform, cf_v2(0, 0)));
			CF_V2 scale_y = cf_sub(cf_mul(transform, cf_v2(0, 1)), cf_mul(transform, cf_v2(0, 0)));
			float scale = cf_max(cf_len(scale_x), cf_len(scale_y));
			float transformed_radius = shape->data.capsule.r * scale;

			// Find the bounds that encompass both endpoints plus the radius
			float min_x = cf_min(a_transformed.x, b_transformed.x) - transformed_radius;
			float max_x = cf_max(a_transformed.x, b_transformed.x) + transformed_radius;
			float min_y = cf_min(a_transformed.y, b_transformed.y) - transformed_radius;
			float max_y = cf_max(a_transformed.y, b_transformed.y) + transformed_radius;

			return cf_make_aabb(cf_v2(min_x, min_y), cf_v2(max_x, max_y));
		} break;
	}
}

static void
bgame_transform_poly(const CF_Poly* input, CF_Poly* output, CF_M3x2 transform) {
	// Assume that this is already a hull since it's done during loading
	for (int i = 0; i < input->count; i++) {
		output->verts[i] = cf_mul(transform, input->verts[i]);
	}
	output->count = input->count;
	cf_norms(output->verts, output->norms, output->count);
}

bgame_collision_shape_t
bgame_transform_collision_shape(const bgame_collision_shape_t* shape, CF_M3x2 transform) {
	switch (shape->type) {
		case CF_SHAPE_TYPE_NONE:
			return *shape;
		case CF_SHAPE_TYPE_AABB: {
			CF_Poly corners = { .count = 4 };
			cf_aabb_verts(&corners.verts[0], shape->data.aabb);

			bgame_collision_shape_t result = {
				.type = CF_SHAPE_TYPE_POLY,
			};
			bgame_transform_poly(&corners, &result.data.poly, transform);
			return result;
		} break;
		case CF_SHAPE_TYPE_POLY: {
			bgame_collision_shape_t result = {
				.type = CF_SHAPE_TYPE_POLY,
			};
			bgame_transform_poly(&shape->data.poly, &result.data.poly, transform);
			return result;
		} break;
		case CF_SHAPE_TYPE_CIRCLE: {
			// Transform the center of the circle
			CF_V2 center_transformed = cf_mul(transform, shape->data.circle.p);

			// The radius gets scaled by the matrix
			// Get the scale from the matrix by transforming a unit vector
			CF_V2 scale_x = cf_sub(cf_mul(transform, cf_v2(1, 0)), cf_mul(transform, cf_v2(0, 0)));
			CF_V2 scale_y = cf_sub(cf_mul(transform, cf_v2(0, 1)), cf_mul(transform, cf_v2(0, 0)));
			float scale = cf_max(cf_len(scale_x), cf_len(scale_y));
			float transformed_radius = shape->data.capsule.r * scale;

			return (bgame_collision_shape_t){
				.type = CF_SHAPE_TYPE_CIRCLE,
				.data.circle = {
					.p = center_transformed,
					.r = transformed_radius,
				},
			};
		} break;
		case CF_SHAPE_TYPE_CAPSULE: {
			// Transform the two endpoints of the capsule
			CF_V2 a_transformed = cf_mul(transform, shape->data.capsule.a);
			CF_V2 b_transformed = cf_mul(transform, shape->data.capsule.b);

			// The radius also gets scaled by the matrix
			// Get the scale from the matrix by transforming a unit vector
			CF_V2 scale_x = cf_sub(cf_mul(transform, cf_v2(1, 0)), cf_mul(transform, cf_v2(0, 0)));
			CF_V2 scale_y = cf_sub(cf_mul(transform, cf_v2(0, 1)), cf_mul(transform, cf_v2(0, 0)));
			float scale = cf_max(cf_len(scale_x), cf_len(scale_y));
			float transformed_radius = shape->data.capsule.r * scale;

			return (bgame_collision_shape_t){
				.type = CF_SHAPE_TYPE_CAPSULE,
				.data.capsule = {
					.a = a_transformed,
					.b = b_transformed,
					.r = transformed_radius,
				},
			};
		} break;
	}
}

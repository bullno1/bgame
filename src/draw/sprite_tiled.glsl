#ifdef BGAME_SHADER_BLIT
// A canvas drawn with this shader is not tiled.
vec4 shader(vec4 color, ShaderParams params) {
	return color;
}
#else
// Repeats the frame over the quad. attributes: xy = grid origin in draw space,
// zw = (cos, sin) of the grid rotation divided by the scale, so a rotated offset
// lands in texels directly.
vec4 shader(vec4 color, ShaderParams params) {
	vec2 d = params.view_pos - params.attributes.xy;
	vec2 cs = params.attributes.zw;
	vec2 t = vec2(cs.x * d.x + cs.y * d.y, -cs.y * d.x + cs.x * d.y); // texels, y up

	vec2 frame = abs((params.uv_max - params.uv_min) * u_texture_size);
	vec2 f = fract(t / frame);
	f.y = 1.0 - f.y; // the stock quad maps fraction 0 to the frame's top
	vec2 uv = mix(params.uv_min, params.uv_max, f);

	// Footprint from the unwrapped coordinate, so the seam gets no derivative spike.
	vec2 fw = fwidth(t);
	vec4 bounds = vec4(min(params.uv_min, params.uv_max), max(params.uv_min, params.uv_max));
	uv = u_use_smooth_uv == 0 ? smooth_uv_fw(uv, bounds, u_texture_size, fw) : uv;

	return texture(u_image, uv) * v_alpha;
}
#endif

layout (set = 3, binding = 1) uniform shd_uniforms {
    vec4 u_overlay_color;
};

vec4 shader(vec4 color, ShaderParams params) {
    return color.rgba * u_overlay_color.rgba;
}

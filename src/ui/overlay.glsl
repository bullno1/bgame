vec4 shader(vec4 color, ShaderParams params) {
    return color * params.attributes;
}

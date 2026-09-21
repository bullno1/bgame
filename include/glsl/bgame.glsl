// Shader helpers for bgame. compile_<type>_shader (bgame.cmake) puts this directory on the
// include path and defines BGAME_SHADER_STAGE as a number. These are the names of those
// numbers: keep them in sync with bgame_compile_shader in bgame.cmake.

#define BGAME_SHADER_STAGE_VERTEX   0
#define BGAME_SHADER_STAGE_FRAGMENT 1
#define BGAME_SHADER_STAGE_COMPUTE  2
#define BGAME_SHADER_STAGE_DRAW     3

// Varying(X): the interface between the vertex and the fragment stage at location X, `out`
// in the former and `in` in the latter, so both can share one list of declarations:
//
//   Varying(0) vec2 v_uv;
//   flat Varying(1) vec4 v_rect;
//
// Left undefined in the stages that have no varyings.
#if BGAME_SHADER_STAGE == BGAME_SHADER_STAGE_VERTEX
#define Varying(X) layout (location = X) out
#elif BGAME_SHADER_STAGE == BGAME_SHADER_STAGE_FRAGMENT
#define Varying(X) layout (location = X) in
#endif

// CF_SAMPLER_SET, CF_UNIFORM_SET: the descriptor sets a stage declares its samplers and its
// uniform block in. SDL_GPU fixes them per stage, so a declaration shared between stages
// cannot spell the number:
//
//   layout (set = CF_SAMPLER_SET, binding = 0) uniform sampler2D u_image;
//   layout (set = CF_UNIFORM_SET, binding = 0) uniform uniform_block { ... };
//
// Storage textures and storage buffers live in the sampler set too, after the samplers; a
// compute shader's read-write ones are apart, in set 1. A draw shader is a fragment shader,
// where uniform binding 0 belongs to CF's built-ins: use binding 1.
#if BGAME_SHADER_STAGE == BGAME_SHADER_STAGE_VERTEX
#define CF_SAMPLER_SET 0
#define CF_UNIFORM_SET 1
#elif BGAME_SHADER_STAGE == BGAME_SHADER_STAGE_COMPUTE
#define CF_SAMPLER_SET 0
#define CF_UNIFORM_SET 2
#else
#define CF_SAMPLER_SET 2
#define CF_UNIFORM_SET 3
#endif

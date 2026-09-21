// Shader helpers for bgame. compile_<type>_shader (bgame.cmake) puts this directory on the
// include path and defines BGAME_SHADER_STAGE as one of the names below.

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

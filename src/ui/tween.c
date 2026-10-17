#include <bgame/ui/tween.h>
#include <bminmax.h>
#include <cute_math.h>

static bgame_ui_tween_t*
tween_state(bgame_ui_tween_tag_t* tag) {
	return bgame_ui__state(sizeof(bgame_ui_tween_t), _Alignof(bgame_ui_tween_t), tag);
}

float
bgame_ui__tween(bgame_ui_tween_tag_t* tag, bgame_ui_tween_desc_t desc) {
	bgame_ui_tween_t* t = tween_state(tag);
	if (!t->started) {
		t->started = true;
		t->value = desc.from_initial ? desc.initial : desc.target;
		t->from = t->value;
		t->to = t->value;
		t->elapsed = desc.duration;
	}
	if (desc.target != t->to) {
		t->from = t->value;
		t->to = desc.target;
		t->elapsed = 0.f;
	}

	t->elapsed += bgame_ui_dt();
	float progress = desc.duration > 0.f ? BMIN(t->elapsed / desc.duration, 1.f) : 1.f;
	bgame_ui_curve_fn_t curve = desc.curve != NULL ? desc.curve : cf_smoothstep;
	// Assigned, not interpolated, at the end: a glide lands on its target to the bit
	t->value = progress >= 1.f ? t->to : cf_lerp(t->from, t->to, curve(progress));
	return t->value;
}

bool
bgame_ui__tween_done(bgame_ui_tween_tag_t* tag) {
	bgame_ui_tween_t* t = tween_state(tag);
	return t->value == t->to;
}

#include <bgame/ui/rich_text.h>
#include <bgame/allocator/frame.h>
#include <cute_draw.h>

static void
bgame_ui_push_text_config(const bgame_ui_rich_text_t* config, float wrap_width) {
	cf_push_font(bgame_get_ui_font_name(config->font_id));
	cf_push_font_size(config->font_size);
	if (config->sizing == BGAME_UI_RICH_TEXT_WRAP) {
		cf_push_text_wrap_width(wrap_width);
	}
}

static void
bgame_ui_pop_text_config(const bgame_ui_rich_text_t* config) {
	if (config->sizing == BGAME_UI_RICH_TEXT_WRAP) {
		cf_pop_text_wrap_width();
	}
	cf_pop_font_size();
	cf_pop_font();
}

static void
bgame_ui_render_rich_text(
	const Clay_RenderCommand* command,
	void* userdata
) {
	const bgame_ui_rich_text_t* rich_text = userdata;
	const float* wrap_width = command->userData;

	bgame_ui_push_text_config(rich_text, *wrap_width);
	cf_push_font_blur(rich_text->font_blur);
	cf_draw_push_color(bgame_ui_color_from_clay(rich_text->color));

	cf_push_text_id(command->id);
	cf_draw_text(
		rich_text->text,
		(CF_V2){ command->boundingBox.x, -command->boundingBox.y },
		rich_text->text_len
	);
	cf_pop_text_id();

	cf_pop_font_blur();
	cf_draw_pop_color();
	bgame_ui_pop_text_config(rich_text);
}

void
bgame_ui_rich_text(
	Clay_ElementId id,
	bgame_ui_rich_text_t rich_text
) {
	Clay_LayoutAlignmentX align_x;
	switch (rich_text.justify) {
		case BGAME_UI_RICH_TEXT_JUSTIFY_LEFT:
			align_x = CLAY_ALIGN_X_LEFT;
			break;
		case BGAME_UI_RICH_TEXT_JUSTIFY_CENTER:
			align_x = CLAY_ALIGN_X_CENTER;
			break;
		case BGAME_UI_RICH_TEXT_JUSTIFY_RIGHT:
			align_x = CLAY_ALIGN_X_RIGHT;
			break;
	}
	BUI(id, {
		.layout = {
			.sizing = {
				.width = rich_text.sizing == BGAME_UI_RICH_TEXT_WRAP
					? CLAY_SIZING_GROW(0)
					: CLAY_SIZING_FIT(0),
				.height = CLAY_SIZING_FIT(0),
			},
			.childAlignment.x = align_x
		},
	}) {
		Clay_ElementData size_data = Clay_GetElementData(id);

		if (size_data.found) {
			bgame_ui_push_text_config(&rich_text, size_data.boundingBox.width);
			CF_V2 size = cf_text_size(rich_text.text, rich_text.text_len);
			bgame_ui_pop_text_config(&rich_text);

			BUI(CLAY_ID_LOCAL("Content"), {
				.layout = {
					.sizing = {
						.width = CLAY_SIZING_FIXED(size.x),
						.height = CLAY_SIZING_FIXED(size.y),
					},
				},
				.custom = bgame_custom_ui_element(
					bgame_ui_render_rich_text,
					bgame_make_frame_copy(rich_text)
				),
				.userData = bgame_make_frame_copy(size_data.boundingBox.width)
			}) {
			}
		}
	}
}

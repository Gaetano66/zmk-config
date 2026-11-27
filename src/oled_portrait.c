#include <string.h>

#include "oled_portrait.h"
#include "oled_portrait_mask.h"

static bool next_glyph(const lv_font_t *font, const char *line, uint32_t length, uint32_t *i,
                       uint32_t *letter, lv_font_glyph_dsc_t *g) {
    *letter = _lv_txt_encoded_next(line, i);
    uint32_t peek = *i;
    uint32_t next = *i < length ? _lv_txt_encoded_next(line, &peek) : 0;
    return lv_font_get_glyph_dsc(font, g, *letter, next);
}

static void draw_line(uint8_t *mask, const char *line, uint32_t length, int y) {
    const lv_font_t *font = &lv_font_montserrat_10;
    int left = 0, right = 0, pen = 0;
    /* Include glyph bearings as well as the advance, so overhangs also fit. */
    for (uint32_t i = 0; i < length;) {
        uint32_t letter;
        lv_font_glyph_dsc_t g;
        if (!next_glyph(font, line, length, &i, &letter, &g)) {
            continue;
        }
        if (g.box_w && g.box_h) {
            int start = pen + g.ofs_x;
            left = LV_MIN(left, start);
            right = LV_MAX(right, start + g.box_w);
        }
        pen += g.adv_w;
    }
    int width = LV_MAX(right, pen) - left;
    int x = -left + LV_MAX(0, CORNE_OLED_LABEL_SIZE - width) / 2;
    for (uint32_t i = 0; i < length;) {
        uint32_t letter;
        lv_font_glyph_dsc_t g;
        if (!next_glyph(font, line, length, &i, &letter, &g)) {
            continue;
        }
        const uint8_t *bitmap = lv_font_get_glyph_bitmap(g.resolved_font, letter);
        if (bitmap) {
            int glyph_y = y + font->line_height - font->base_line - g.box_h - g.ofs_y;
            corne_oled_stamp_glyph(mask, x + g.ofs_x, glyph_y, g.box_w, g.box_h, g.bpp, bitmap,
                                   width);
        }
        x += g.adv_w;
    }
}

static void draw_portrait_label(lv_event_t *event) {
    lv_obj_t *label = lv_event_get_target(event);
    const char *line = lv_label_get_text(label);
    const char *newline = strchr(line, '\n');
    uint8_t mask[CORNE_OLED_LABEL_BYTES] = {0};
    int line_height = lv_font_montserrat_10.line_height;
    int y = (CORNE_OLED_LABEL_SIZE - (newline ? 2 : 1) * line_height) / 2;
    draw_line(mask, line, newline ? (uint32_t)(newline - line) : strlen(line), y);
    if (newline) {
        draw_line(mask, newline + 1, strlen(newline + 1), y + line_height);
    }
    lv_img_dsc_t image = {
        .header = {.cf = LV_IMG_CF_ALPHA_1BIT, .w = CORNE_OLED_LABEL_SIZE, .h = CORNE_OLED_LABEL_SIZE},
        .data_size = sizeof(mask),
        .data = mask,
    };
    lv_draw_img_dsc_t dsc;
    lv_draw_img_dsc_init(&dsc);
    dsc.recolor = lv_obj_get_style_text_color(label, LV_PART_MAIN);
    lv_area_t area;
    lv_obj_get_coords(label, &area);
    lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(event);
    lv_draw_img(ctx, &dsc, &area, &image);
    lv_draw_wait_for_finish(ctx);
    lv_img_cache_invalidate_src(&image);
}

void corne_oled_portrait_label(lv_obj_t *label, lv_coord_t x_offset) {
    /* Keep stock labels and their live text; draw our own 1-bit rotated glyph mask. */
    lv_obj_set_size(label, CORNE_OLED_LABEL_SIZE, CORNE_OLED_LABEL_SIZE);
    lv_obj_set_style_text_opa(label, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_align(label, LV_ALIGN_CENTER, x_offset, 0);
    lv_obj_add_event_cb(label, draw_portrait_label, LV_EVENT_DRAW_MAIN, NULL);
}

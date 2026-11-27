#pragma once

#include <lvgl.h>

/* Match the clockwise orientation of the cat without rotating the display driver. */
void corne_oled_portrait_label(lv_obj_t *label, lv_coord_t x_offset);

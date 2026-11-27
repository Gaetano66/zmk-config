#define DT_DRV_COMPAT zmk_behavior_corne_rgb_fx
#include <zephyr/device.h>
#include <zephyr/sys/atomic.h>
#include <drivers/behavior.h>
#include <zmk/keymap.h>
#include <zmk/rgb_underglow.h>
#include "rgb_fx_engine.h"

/* 0..3 are native solid/breathe/spectrum/swirl. Modes 4..6 filter solid. */
static atomic_t extra_mode;
unsigned corne_rgb_extra_mode(void) { return atomic_get(&extra_mode); }

static int normalize(struct zmk_behavior_binding *b, struct zmk_behavior_binding_event e) {
    unsigned cmd = b->param1;
    if (cmd == 0) {
        unsigned mode = corne_rgb_extra_mode();
        if (!mode) { mode = zmk_rgb_underglow_calc_effect(0); }
        b->param1 = (mode + 1) % 7;
        b->param2 = 0;
    } else if (cmd == 1 || cmd == 2) {
        struct zmk_led_hsb color = zmk_rgb_underglow_calc_brt(0);
        b->param1 = 0;
        /* Nonzero tag distinguishes choosing red from no color update. */
        b->param2 = 1 + fx_palette_step(color.h, cmd == 1 ? -1 : 1);
    } else { return -EINVAL; }
    return 0;
}

static int press(struct zmk_behavior_binding *b, struct zmk_behavior_binding_event e) {
    if (b->param1 > 6 || b->param2 > 360) { return -EINVAL; }
    atomic_set(&extra_mode, b->param1 >= 4 ? b->param1 : 0);
    if (b->param2) {
        struct zmk_led_hsb color = zmk_rgb_underglow_calc_brt(0);
        color.h = b->param2 - 1;
        color.s = 100;
        zmk_rgb_underglow_set_hsb(color);
    }
    return zmk_rgb_underglow_select_effect(b->param1 >= 4 ? 0 : b->param1);
}
static int release(struct zmk_behavior_binding *b, struct zmk_behavior_binding_event e) {
    return ZMK_BEHAVIOR_OPAQUE;
}
static const struct behavior_driver_api api = {
    .locality = BEHAVIOR_LOCALITY_GLOBAL,
    .binding_convert_central_state_dependent_params = normalize,
    .binding_pressed = press, .binding_released = release,
};
BEHAVIOR_DT_INST_DEFINE(0, NULL, NULL, NULL, NULL, POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &api);

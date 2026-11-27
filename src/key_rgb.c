#include <zephyr/device.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/sys/atomic.h>
#include <drivers/behavior.h>
#include <zmk/keymap.h>

#include "key_rgb_frame.h"
#include "pomodoro.h"
#include "pomodoro_progress.h"
#include "rgb_fx_engine.h"
unsigned corne_rgb_extra_mode(void);

#define DT_DRV_COMPAT zmk_behavior_key_rgb
#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

#define PROXY DT_CHOSEN(zmk_underglow)
#define STRIP DT_PHANDLE(PROXY, strip)
BUILD_ASSERT(DT_NODE_HAS_COMPAT(PROXY, zmk_key_rgb_strip));
BUILD_ASSERT(DT_PROP(PROXY, chain_length) == STRIP_PIXELS);
BUILD_ASSERT(DT_PROP(STRIP, chain_length) == STRIP_PIXELS);
BUILD_ASSERT((int)STRIP_PIXELS == (int)POMO_RGB_LEDS);

/* Volatile across boots; six underglow LEDs are visible by default. */
static atomic_t keys_enabled;

static int update_rgb(const struct device *dev, struct led_rgb *pixels, size_t count) {
    struct led_rgb frame[STRIP_PIXELS];
    int err = key_rgb_frame(frame, pixels, count, atomic_get(&keys_enabled));
    if (err) { return err; }
    unsigned mode = corne_rgb_extra_mode();
    unsigned pct = corne_pomodoro_rgb_progress();
    if (mode || pct != POMO_RGB_INACTIVE) {
        const bool right = IS_ENABLED(CONFIG_SHIELD_CORNE_RIGHT);
        unsigned tick = k_uptime_get_32() / 100;
        for (size_t i = 0; i < count; ++i) {
            unsigned g = pomodoro_rgb_gain(right, i, pct);
            if (mode) {
                g = g * fx_gain(mode, i, tick) / 255;
            }
            frame[i].r = frame[i].r * g / 255;
            frame[i].g = frame[i].g * g / 255;
            frame[i].b = frame[i].b * g / 255;
        }
    }
    if (corne_pomodoro_dim_rgb()) {
        /* Attenuation only: never light an off strip or a disabled zone. */
        memset(frame, 0, sizeof(frame));
    }
    return led_strip_update_rgb(DEVICE_DT_GET(STRIP), frame, count);
}

static int update_channels(const struct device *dev, uint8_t *channels, size_t count) {
    return -ENOTSUP;
}

static const struct led_strip_driver_api strip_api = {
    .update_rgb = update_rgb,
    .update_channels = update_channels,
};

static int proxy_init(const struct device *dev) {
    return device_is_ready(DEVICE_DT_GET(STRIP)) ? 0 : -ENODEV;
}

/* The physical WS2812 device initializes at POST_KERNEL priority 90. */
#define PROXY_INIT_PRIORITY 91
BUILD_ASSERT(CONFIG_LED_STRIP_INIT_PRIORITY < PROXY_INIT_PRIORITY);
DEVICE_DT_DEFINE(PROXY, proxy_init, NULL, NULL, NULL, POST_KERNEL, PROXY_INIT_PRIORITY, &strip_api);

/* ZMK converts on the central before dispatch. Send a set, never a toggle,
 * so a stale peripheral converges on the next press. Releases do nothing. */
static int normalize(struct zmk_behavior_binding *binding, struct zmk_behavior_binding_event event) {
    binding->param1 = !atomic_get(&keys_enabled);
    return 0;
}

static int press(struct zmk_behavior_binding *binding, struct zmk_behavior_binding_event event) {
    atomic_set(&keys_enabled, !!binding->param1);
    return ZMK_BEHAVIOR_OPAQUE;
}

static int release(struct zmk_behavior_binding *binding, struct zmk_behavior_binding_event event) {
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_api = {
    .locality = BEHAVIOR_LOCALITY_GLOBAL,
    .binding_convert_central_state_dependent_params = normalize,
    .binding_pressed = press,
    .binding_released = release,
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
    .get_parameter_metadata = zmk_behavior_get_empty_param_metadata,
#endif
};

BEHAVIOR_DT_INST_DEFINE(0, NULL, NULL, NULL, NULL, POST_KERNEL,
                        CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &behavior_api);
#endif

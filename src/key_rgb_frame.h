#pragma once

#include <errno.h>
#include <stdbool.h>
#include <string.h>

/* The board routes data through the six underglow LEDs before the switch LEDs. */
enum { UNDERGLOW_PIXELS = 6, STRIP_PIXELS = 27 };

static inline int key_rgb_frame(struct led_rgb *out, const struct led_rgb *in,
                                size_t count, bool keys_enabled) {
    if (count != STRIP_PIXELS) {
        return -EINVAL;
    }
    size_t lit = keys_enabled ? count : UNDERGLOW_PIXELS;
    memcpy(out, in, lit * sizeof(*out));
    memset(out + lit, 0, (count - lit) * sizeof(*out));
    return 0;
}

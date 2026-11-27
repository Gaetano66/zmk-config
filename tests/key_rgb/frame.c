/* Run: cc -std=c11 -Wall -Wextra -Werror tests/key_rgb/frame.c -o "$TMPDIR/key-rgb-test" && "$TMPDIR/key-rgb-test" */
#include <assert.h>
#include <stdint.h>
#include <string.h>

/* Match the padded Zephyr LED layout too; don't assume three bytes per pixel. */
struct led_rgb { uint8_t scratch, r, g, b; };
#include "../../src/key_rgb_frame.h"

int main(void) {
    struct led_rgb input[27], original[27], out[27];
    for (int i = 0; i < 27; ++i) {
        input[i] = (struct led_rgb){0, i + 1, i + 2, i + 3};
    }
    memcpy(original, input, sizeof input);
    for (int enabled = 0; enabled <= 1; ++enabled) {
        assert(key_rgb_frame(out, input, 27, enabled) == 0);
        for (int i = 0; i < 27; ++i) {
            struct led_rgb expected = enabled || i < 6 ? original[i] : (struct led_rgb){0};
            assert(!memcmp(&out[i], &expected, sizeof expected));
        }
        memset(out, 0xff, sizeof out); /* A physical strip driver may overwrite its input. */
        assert(!memcmp(input, original, sizeof input));
    }
    assert(key_rgb_frame(out, input, 28, 1) == -EINVAL);
    assert(key_rgb_frame(out, input, 26, 1) == -EINVAL);
    memset(input, 0, sizeof input); /* ZMK master off must still black out both zones. */
    assert(key_rgb_frame(out, input, 27, 1) == 0);
    assert(!memcmp(input, out, sizeof input));
}

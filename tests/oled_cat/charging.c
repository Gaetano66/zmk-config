#include <assert.h>

#include "../../src/oled_cat_charging.h"

static void only_powered_wake_replays(void) {
    struct cat_charging state = {0};
    cat_charging_replay(&state);
    struct cat_display frame = cat_charging_update(&state, false, 0, 7);
    assert(frame.mood == 7 && !frame.sparkle);
    frame = cat_charging_update(&state, true, 100, 7);
    assert(frame.mood == 7 && !frame.sparkle);  /* Plugging in after wake isn't a trigger. */
    frame = cat_charging_update(&state, true, 1000, 5);
    assert(frame.mood == 5 && !frame.sparkle);  /* Neither is a new charge reading. */

    cat_charging_replay(&state);
    assert(cat_charging_update(&state, true, 2000, 7).mood == 9);
    assert(cat_charging_update(&state, true, 2249, 7).mood == 9);
    assert(cat_charging_update(&state, true, 2250, 7).mood == 8);
    frame = cat_charging_update(&state, true, 2500, 7);
    assert(frame.mood == 7 && frame.sparkle);
    frame = cat_charging_update(&state, true, 3250, 7);
    assert(frame.mood == 7 && !frame.sparkle);
    frame = cat_charging_update(&state, true, 3300, 5);
    assert(frame.mood == 5 && !frame.sparkle);

    cat_charging_replay(&state);
    assert(cat_charging_update(&state, true, 4000, 5).mood == 9);
    frame = cat_charging_update(&state, false, 4050, 5);
    assert(frame.mood == 5 && !frame.sparkle);  /* Losing power ends the animation. */
    frame = cat_charging_update(&state, true, 5000, 5);
    assert(frame.mood == 5 && !frame.sparkle);
}

int main(void) {
    only_powered_wake_replays();
    for (unsigned target = 0; target < CAT_MOOD_COUNT; ++target) {
        struct cat_charging session = {0};
        unsigned last = CAT_MOOD_COUNT - 1;
        struct cat_display frame = {0};
        cat_charging_replay(&session);
        for (int64_t now = 0; now <= 4000; now += 250) {
            frame = cat_charging_update(&session, true, now, target);
            assert(frame.mood >= target && frame.mood <= last);
            last = frame.mood;
        }
        assert(last == target && !frame.sparkle);
    }

    uint8_t source[CAT_BYTES] = {0};
    uint8_t normal[CAT_OLED_BYTES], celebration[CAT_OLED_BYTES];
    source[0] = 0x80;
    source[CAT_BYTES - 1] = 0x10;
    cat_oled_expand(source, normal, false);
    cat_oled_expand(source, celebration, true);
    assert(CAT_OLED_WIDTH == 42 && CAT_HEIGHT == 32);
    assert(normal[0] == 0xc0 && normal[CAT_OLED_BYTES - 1] == 0x40);
    assert(celebration[0] == (normal[0] | 0x10));
    for (unsigned y = 0; y < CAT_HEIGHT; ++y) {
        assert((normal[(y + 1) * CAT_OLED_STRIDE - 1] & 0x3f) == 0);
    }
}

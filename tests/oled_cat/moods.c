#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../../src/oled_cat_frames.h"

/* Sample the rendered masks in upright art coordinates. */
static bool upright_pixel(const uint8_t *frame, unsigned x, unsigned y) {
    unsigned column = CAT_WIDTH - 1 - y;
    return frame[x * CAT_STRIDE + column / 8] & (0x80u >> (column % 8));
}

static void check_ears_and_whiskers(const uint8_t *frame, const uint8_t *previous) {
    bool ears_changed = false;
    bool whiskers_changed = false;
    unsigned ear_tip = 8;
    unsigned previous_tip = 8;
    unsigned whisker_end = 0;
    unsigned previous_end = 0;
    for (unsigned y = 0; y < 8; ++y) {
        for (unsigned x = 0; x < CAT_HEIGHT; ++x) {
            bool pixel = upright_pixel(frame, x, y);
            bool old = upright_pixel(previous, x, y);
            ears_changed |= pixel != old;
            if (pixel && y < ear_tip) { ear_tip = y; }
            if (old && y < previous_tip) { previous_tip = y; }
        }
    }
    for (unsigned y = 13; y < CAT_WIDTH; ++y) {
        for (unsigned x = 0; x < 4; ++x) {
            bool pixel = upright_pixel(frame, x, y);
            bool old = upright_pixel(previous, x, y);
            whiskers_changed |= pixel != old;
            if (pixel) { whisker_end = y; }
            if (old) { previous_end = y; }
        }
    }
    assert(ears_changed && ear_tip >= previous_tip);
    assert(whiskers_changed && whisker_end >= previous_end);
}

int main(int argc, char **argv) {
    uint8_t frames[CAT_MOOD_COUNT][CAT_BYTES];
    uint8_t blink[CAT_BYTES];
    for (unsigned percent = 0; percent <= 100; ++percent) {
        unsigned expected = percent == 0 ? 9 : (100 - percent) / 10;
        assert(cat_mood_for_charge(percent) == expected);
    }
    assert(cat_mood_for_charge(255) == 0);
    assert(CAT_MOOD_COUNT == 10 && CAT_WIDTH == 28 && CAT_HEIGHT == 32);

    for (unsigned mood = 0; mood < CAT_MOOD_COUNT; ++mood) {
        cat_make_frames(mood, frames[mood], blink);
        assert(memcmp(frames[mood], blink, CAT_BYTES) != 0);
        if (mood > 0) {
            check_ears_and_whiskers(frames[mood], frames[mood - 1]);
        }
        for (unsigned previous = 0; previous < mood; ++previous) {
            assert(memcmp(frames[mood], frames[previous], CAT_BYTES) != 0);
        }
        for (unsigned y = 0; y < CAT_HEIGHT; ++y) {
            for (unsigned x = 0; x < CAT_WIDTH; ++x) {
                unsigned i = y * CAT_STRIDE + x / 8;
                unsigned mask = 0x80u >> (x % 8);
                bool eyelid = x >= 11 && x <= 14 &&
                              ((y >= 10 && y <= 13) || (y >= 19 && y <= 22));
                bool open_on = frames[mood][i] & mask;
                bool blink_on = blink[i] & mask;
                assert(eyelid ? blink_on == (x == 12) : blink_on == open_on);
            }
            assert((frames[mood][y * CAT_STRIDE + 3] & 0x0f) == 0);
            assert((blink[y * CAT_STRIDE + 3] & 0x0f) == 0);
        }
        if (argc == 2 && strcmp(argv[1], "--dump") == 0) {
            for (unsigned byte = 0; byte < CAT_BYTES; ++byte) {
                printf("%02x", frames[mood][byte]);
            }
            puts("");
        }
    }
}

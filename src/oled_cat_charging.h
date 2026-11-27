#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "oled_cat_frames.h"

enum { CAT_CHARGE_STEP_MS = 250, CAT_CHARGE_SPARKLE_MS = 750 };

struct cat_charging {
    bool replay_pending;
    bool animating;
    unsigned mood;
    int64_t next_step_at;
    int64_t sparkle_until;
};

struct cat_display {
    unsigned mood;
    bool sparkle;
};

static inline void cat_charging_replay(struct cat_charging *state) {
    state->replay_pending = true;
}

static inline struct cat_display cat_charging_update(struct cat_charging *state, bool powered,
                                                     int64_t now, unsigned real_mood) {
    if (!powered) {
        *state = (struct cat_charging){0};
        return (struct cat_display){.mood = real_mood};
    }
    if (state->replay_pending) {
        *state = (struct cat_charging){.animating = true, .mood = CAT_MOOD_COUNT - 1};
        if (state->mood == real_mood) {
            state->sparkle_until = now + CAT_CHARGE_SPARKLE_MS;
        }
    }
    if (!state->animating) {
        return (struct cat_display){.mood = real_mood};
    }
    if (state->mood < real_mood) {
        /* A lower reading must take precedence over any decorative transition. */
        state->mood = real_mood;
        state->sparkle_until = 0;
    }
    if (state->mood > real_mood) {
        state->sparkle_until = 0;
        if (!state->next_step_at) {
            state->next_step_at = now + CAT_CHARGE_STEP_MS;
        } else if (now >= state->next_step_at) {
            --state->mood;
            state->next_step_at = now + CAT_CHARGE_STEP_MS;
            if (state->mood == real_mood) {
                state->sparkle_until = now + CAT_CHARGE_SPARKLE_MS;
            }
        }
    }
    if (state->mood == real_mood) {
        state->next_step_at = 0;
    }
    bool sparkle = now < state->sparkle_until;
    if (state->mood == real_mood && !sparkle) {
        state->animating = false;
    }
    return (struct cat_display){.mood = state->mood, .sparkle = sparkle};
}

/* The rotated portrait already fills 32 rows. Expand horizontally around its center. */
enum { CAT_OLED_WIDTH = 42, CAT_OLED_STRIDE = (CAT_OLED_WIDTH + 7) / 8,
       CAT_OLED_BYTES = CAT_OLED_STRIDE * CAT_HEIGHT };

static inline void cat_oled_pixel(uint8_t dest[CAT_OLED_BYTES], unsigned x, unsigned y) {
    dest[y * CAT_OLED_STRIDE + x / 8] |= 0x80u >> (x % 8);
}

static inline void cat_oled_expand(const uint8_t source[CAT_BYTES], uint8_t dest[CAT_OLED_BYTES],
                                   bool sparkle) {
    memset(dest, 0, CAT_OLED_BYTES);
    for (unsigned y = 0; y < CAT_HEIGHT; ++y) {
        for (unsigned x = 0; x < CAT_OLED_WIDTH; ++x) {
            unsigned sample = x * CAT_WIDTH / CAT_OLED_WIDTH;
            if (source[y * CAT_STRIDE + sample / 8] & (0x80u >> (sample % 8))) {
                cat_oled_pixel(dest, x, y);
            }
        }
    }
    if (sparkle) {
        /* Mark the temporary happy finale rather than suggesting a sudden full charge. */
        cat_oled_pixel(dest, 3, 0);
        cat_oled_pixel(dest, 2, 1);
        cat_oled_pixel(dest, 3, 1);
        cat_oled_pixel(dest, 4, 1);
        cat_oled_pixel(dest, 3, 2);
        cat_oled_pixel(dest, 7, 4);
    }
}

#pragma once

/* Nearest palette step; wrapping works in both directions despite integer hues. */
static inline unsigned fx_palette_step(unsigned hue, int direction) {
    unsigned index = ((hue % 360) * 32 + 180) / 360;
    return ((index + 32 + direction) % 32) * 360 / 32;
}

/* Gain in 0..255. Native effects pass through; the three added animations
 * attenuate native solid-color frames, so blanked/masked pixels stay black. */
static inline unsigned fx_gain(unsigned mode, unsigned pixel, unsigned tick) {
    if (mode == 4) { /* Comet, with a four-pixel fading tail */
        unsigned distance = (tick % 27 + 27 - pixel % 27) % 27;
        return distance < 5 ? 255u >> distance : 12u;
    }
    if (mode == 5) { /* Marquee: alternating groups of three */
        return ((pixel / 3 + tick / 4) & 1) ? 36 : 255;
    }
    if (mode == 6) { /* Traveling triangular brightness wave */
        unsigned phase = (pixel * 2 + tick) % 32;
        return 15 + 15 * (phase < 16 ? phase : 32 - phase);
    }
    return 255;
}

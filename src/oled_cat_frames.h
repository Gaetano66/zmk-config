#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum { CAT_WIDTH = 28, CAT_HEIGHT = 32, CAT_STRIDE = (CAT_WIDTH + 7) / 8,
       CAT_BYTES = CAT_HEIGHT * CAT_STRIDE, CAT_MOOD_COUNT = 10 };

/* Face without ears or whiskers; rotated 90 degrees clockwise, MSB first. */
static const uint8_t cat_face_base[] = {
    0x00, 0x00, 0x00, 0x00, /* ............................ */
    0x00, 0x00, 0x00, 0x00, /* ............................ */
    0x00, 0x00, 0x00, 0x00, /* ............................ */
    0x00, 0x00, 0x00, 0x00, /* ............................ */
    0x01, 0xe0, 0x00, 0x00, /* .......####................. */
    0x06, 0x1f, 0x80, 0x00, /* .....##....######........... */
    0x08, 0x00, 0x00, 0x00, /* ....#....................... */
    0x10, 0x00, 0x00, 0x00, /* ...#........................ */
    0x20, 0x00, 0x00, 0x00, /* ..#......................... */
    0x20, 0x00, 0x00, 0x00, /* ..#......................... */
    0x20, 0x00, 0x00, 0x00, /* ..#......................... */
    0x40, 0x1e, 0x00, 0x00, /* .#.........####............. */
    0x42, 0x1e, 0x00, 0x00, /* .#....#....####............. */
    0x44, 0x00, 0x00, 0x00, /* .#...#...................... */
    0x44, 0x40, 0x00, 0x00, /* .#...#...#.................. */
    0x43, 0xc0, 0x00, 0x00, /* .#....####.................. */
    0x43, 0xc0, 0x00, 0x00, /* .#....####.................. */
    0x44, 0x40, 0x00, 0x00, /* .#...#...#.................. */
    0x44, 0x00, 0x00, 0x00, /* .#...#...................... */
    0x42, 0x00, 0x00, 0x00, /* .#....#..................... */
    0x40, 0x1e, 0x00, 0x00, /* .#.........####............. */
    0x40, 0x1e, 0x00, 0x00, /* .#.........####............. */
    0x20, 0x00, 0x00, 0x00, /* ..#......................... */
    0x20, 0x00, 0x00, 0x00, /* ..#......................... */
    0x10, 0x00, 0x00, 0x00, /* ...#........................ */
    0x08, 0x00, 0x00, 0x00, /* ....#....................... */
    0x07, 0x1f, 0x80, 0x00, /* .....###...######........... */
    0x00, 0xe0, 0x00, 0x00, /* ........###................. */
    0x00, 0x00, 0x00, 0x00, /* ............................ */
    0x00, 0x00, 0x00, 0x00, /* ............................ */
    0x00, 0x00, 0x00, 0x00, /* ............................ */
    0x00, 0x00, 0x00, 0x00, /* ............................ */
};
_Static_assert(sizeof(cat_face_base) == CAT_BYTES, "Invalid cat mask size");

enum { CAT_MOUTH_X = 10, CAT_MOUTH_Y = 20, CAT_MOUTH_WIDTH = 11, CAT_MOUTH_HEIGHT = 5 };

/* Smile to frown, in upright art coordinates. Bit 0 is the left edge. */
static const uint16_t cat_mouths[CAT_MOOD_COUNT][CAT_MOUTH_HEIGHT] = {
    {0x401, 0x202, 0x104, 0x0f8, 0x000},
    {0x202, 0x104, 0x0f8, 0x000, 0x000},
    {0x000, 0x104, 0x0f8, 0x000, 0x000},
    {0x000, 0x088, 0x070, 0x000, 0x000},
    {0x000, 0x000, 0x1fc, 0x000, 0x000},
    {0x000, 0x0f8, 0x104, 0x000, 0x000},
    {0x000, 0x070, 0x088, 0x104, 0x000},
    {0x000, 0x0f8, 0x104, 0x202, 0x000},
    {0x000, 0x0f8, 0x104, 0x202, 0x1fc},
    {0x000, 0x070, 0x18c, 0x202, 0x1fc},
};

static inline unsigned cat_mood_for_charge(unsigned percent) {
    unsigned mood = percent >= 100 ? 0 : (100 - percent) / 10;
    return mood >= CAT_MOOD_COUNT ? CAT_MOOD_COUNT - 1 : mood;
}

/* Use upright art coordinates for expressions and rotate into the displayed mask. */
static inline void cat_face_pixel(uint8_t *pixels, unsigned x, unsigned y, bool on) {
    unsigned col = CAT_WIDTH - 1 - y;
    uint8_t *byte = &pixels[x * CAT_STRIDE + col / 8];
    uint8_t mask = 0x80u >> (col % 8);
    *byte = on ? *byte | mask : *byte & ~mask;
}

struct cat_pose {
    uint8_t ear_x, ear_y;
    uint8_t whisker_top, whisker_bottom;
};

/* As charge falls, ears tilt out and both whiskers droop. */
static const struct cat_pose cat_poses[CAT_MOOD_COUNT] = {
    {7, 1, 14, 17}, {7, 2, 15, 18}, {6, 2, 16, 19}, {6, 3, 17, 19}, {5, 4, 18, 20},
    {5, 5, 19, 21}, {4, 5, 19, 22}, {3, 6, 20, 23}, {2, 7, 21, 24}, {1, 8, 21, 26},
};

/* Draw a line in the left half of the face and its mirror on the right. */
static inline void cat_face_line_pair(uint8_t *pixels, int x, int y, int end_x, int end_y) {
    int dx = abs(end_x - x), sx = x < end_x ? 1 : -1;
    int dy = -abs(end_y - y), sy = y < end_y ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        cat_face_pixel(pixels, x, y, true);
        cat_face_pixel(pixels, CAT_HEIGHT - 1 - x, y, true);
        if (x == end_x && y == end_y) {
            return;
        }
        int step = 2 * err;
        if (step >= dy) {
            err += dy;
            x += sx;
        }
        if (step <= dx) {
            err += dx;
            y += sy;
        }
    }
}

static inline void cat_draw_pose(uint8_t *pixels, unsigned mood) {
    const struct cat_pose *pose = &cat_poses[mood];
    cat_face_line_pair(pixels, 5, 11, pose->ear_x, pose->ear_y);
    cat_face_line_pair(pixels, pose->ear_x, pose->ear_y, 13, 8);
    cat_face_line_pair(pixels, 13, 8, 15, 8);
    cat_face_line_pair(pixels, pose->ear_x + 1, pose->ear_y + 2, 8, 10);
    cat_face_line_pair(pixels, 0, pose->whisker_top, 7, 18);
    cat_face_line_pair(pixels, 1, pose->whisker_bottom, 8, 20);
}

static inline void cat_make_frames(unsigned mood, uint8_t open[CAT_BYTES], uint8_t blink[CAT_BYTES]) {
    memcpy(open, cat_face_base, CAT_BYTES);
    cat_draw_pose(open, mood);
    for (unsigned y = 0; y < CAT_MOUTH_HEIGHT; ++y) {
        for (unsigned x = 0; x < CAT_MOUTH_WIDTH; ++x) {
            cat_face_pixel(open, CAT_MOUTH_X + x, CAT_MOUTH_Y + y,
                           cat_mouths[mood][y] & (1u << x));
        }
    }
    if (mood >= 5) {
        /* Raised inner eyebrows; deeper slant at low charge. */
        for (unsigned i = 0; i < 4; ++i) {
            unsigned y = 12 - (mood >= 7 ? i : i / 2);
            cat_face_pixel(open, 10 + i, y, true);
            cat_face_pixel(open, 21 - i, y, true);
        }
    }
    if (mood >= 7) {
        unsigned last = mood == 7 ? 18 : mood == 8 ? 19 : 22;
        for (unsigned y = 17; y <= last; ++y) {
            cat_face_pixel(open, 10, y, true);
            if (mood >= 8) {
                cat_face_pixel(open, 21, y, true);
            }
            if (mood == 9 && y >= 19) {
                cat_face_pixel(open, 9, y, true);
                cat_face_pixel(open, 22, y, true);
            }
        }
    }
    memcpy(blink, open, CAT_BYTES);
    for (unsigned y = 13; y <= 16; ++y) {
        for (unsigned x = 10; x <= 13; ++x) {
            cat_face_pixel(blink, x, y, y == 15);
            cat_face_pixel(blink, x + 9, y, y == 15);
        }
    }
}

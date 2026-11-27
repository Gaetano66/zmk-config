#pragma once

#include <stdbool.h>
#include <zephyr/sys/util.h>

unsigned corne_pomodoro_rgb_progress(void);

#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL) && IS_ENABLED(CONFIG_ZMK_DISPLAY)
bool corne_pomodoro_dim_rgb(void);
#else
static inline bool corne_pomodoro_dim_rgb(void) { return false; }
#endif

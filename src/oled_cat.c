#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <lvgl.h>
#include <hal/nrf_power.h>
#include <zmk/display.h>
#include <zmk/battery.h>
#include <zmk/event_manager.h>
#include <zmk/activity.h>
#include <zmk/events/activity_state_changed.h>
#include <zmk/events/battery_state_changed.h>

#include "oled_cat_charging.h"

enum { CAT_BOOT_TRIES = 20, CAT_OPEN_MS = 4000, CAT_BLINK_MS = 160, CAT_POWER_SAMPLE_MS = 100 };

static uint8_t open_pixels[CAT_OLED_BYTES];
static uint8_t blink_pixels[CAT_OLED_BYTES];
static const lv_img_dsc_t cat_image = {
    .header = {.cf = LV_IMG_CF_ALPHA_1BIT, .w = CAT_OLED_WIDTH, .h = CAT_HEIGHT},
    .data_size = sizeof(open_pixels),
    .data = open_pixels,
};

static const lv_img_dsc_t blink_image = {
    .header = {.cf = LV_IMG_CF_ALPHA_1BIT, .w = CAT_OLED_WIDTH, .h = CAT_HEIGHT},
    .data_size = sizeof(blink_pixels),
    .data = blink_pixels,
};
static bool eyes_closed;
static lv_obj_t *cat_obj;
static unsigned displayed_mood = CAT_MOOD_COUNT;
static unsigned real_mood;
static bool displayed_sparkle;
static struct cat_charging charging;

static void update_cat_charge(uint8_t percent) {
    real_mood = cat_mood_for_charge(percent);
}

static void refresh_cat(void) {
    /* VBUS works even on the peripheral or on a charger without USB data. */
    bool powered = nrf_power_usbregstatus_vbusdet_get(NRF_POWER);
    struct cat_display frame = cat_charging_update(&charging, powered, k_uptime_get(), real_mood);
    if (frame.mood != displayed_mood || frame.sparkle != displayed_sparkle) {
        uint8_t open[CAT_BYTES], blink[CAT_BYTES];
        cat_make_frames(frame.mood, open, blink);
        cat_oled_expand(open, open_pixels, frame.sparkle);
        cat_oled_expand(blink, blink_pixels, frame.sparkle);
        displayed_mood = frame.mood;
        displayed_sparkle = frame.sparkle;
        lv_img_cache_invalidate_src(&cat_image);
        lv_img_cache_invalidate_src(&blink_image);
        lv_obj_invalidate(cat_obj);
    }
    lv_obj_clear_flag(cat_obj, LV_OBJ_FLAG_HIDDEN);
}

static void watch_cat_power(lv_timer_t *timer) {
    ARG_UNUSED(timer);
    refresh_cat();
}

static void replay_on_wake(struct k_work *work) {
    ARG_UNUSED(work);
    if (cat_obj && zmk_activity_get_state() == ZMK_ACTIVITY_ACTIVE) {
        cat_charging_replay(&charging);
        refresh_cat();
    }
}
K_WORK_DEFINE(cat_wake_work, replay_on_wake);

static int cat_activity_changed(const zmk_event_t *event) {
    const struct zmk_activity_state_changed *change = as_zmk_activity_state_changed(event);
    if (IS_ENABLED(CONFIG_ZMK_DISPLAY_BLANK_ON_IDLE) && change &&
        change->state == ZMK_ACTIVITY_ACTIVE) {
        /* Activity fires on transitions, not on every key while the display is awake. */
        k_work_submit_to_queue(zmk_display_work_q(), &cat_wake_work);
    }
    return ZMK_EV_EVENT_BUBBLE;
}
ZMK_LISTENER(cat_activity, cat_activity_changed);
ZMK_SUBSCRIPTION(cat_activity, zmk_activity_state_changed);

static uint8_t get_cat_charge(const zmk_event_t *event) {
    const struct zmk_battery_state_changed *change = as_zmk_battery_state_changed(event);
    return change ? change->state_of_charge : zmk_battery_state_of_charge();
}

ZMK_DISPLAY_WIDGET_LISTENER(cat_battery, uint8_t, update_cat_charge, get_cat_charge)
ZMK_SUBSCRIPTION(cat_battery, zmk_battery_state_changed);

static void draw_cat(lv_event_t *event) {
    lv_obj_t *obj = lv_event_get_target(event);
    lv_draw_img_dsc_t dsc;
    lv_draw_img_dsc_init(&dsc);
    dsc.recolor = lv_obj_get_style_text_color(obj, LV_PART_MAIN);
    lv_area_t area;
    lv_obj_get_coords(obj, &area);
    lv_draw_img(lv_event_get_draw_ctx(event), &dsc, &area, eyes_closed ? &blink_image : &cat_image);
}

/* LVGL runs this alongside its normal display tick; it never wakes an idle OLED. */
static void blink_cat(lv_timer_t *timer) {
    eyes_closed = !eyes_closed;
    lv_obj_invalidate(timer->user_data);
    lv_timer_set_period(timer, eyes_closed ? CAT_BLINK_MS : CAT_OPEN_MS);
}

static lv_timer_t *power_timer;

static void delete_cat(lv_event_t *event) {
    lv_timer_t *timer = lv_event_get_user_data(event);
    if (timer) {
        lv_timer_del(timer);
    }
    if (power_timer) {
        lv_timer_del(power_timer);
        power_timer = NULL;
    }
    cat_obj = NULL;
    displayed_mood = CAT_MOOD_COUNT;
    charging = (struct cat_charging){0};
}

static void attach_cat(struct k_work *work) {
    ARG_UNUSED(work);
    /* The normal widgets remain in the corners; Pomodoro uses LVGL's top layer. */
    lv_obj_t *screen = lv_scr_act();
    lv_obj_t *cat = lv_obj_create(screen);
    if (!cat) {
        return;
    }
    lv_obj_remove_style_all(cat);
    lv_obj_set_size(cat, CAT_OLED_WIDTH, CAT_HEIGHT);
    lv_obj_clear_flag(cat, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    /* Use the same upper end of the portrait OLED on both halves. */
    lv_obj_align(cat, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_add_event_cb(cat, draw_cat, LV_EVENT_DRAW_MAIN, NULL);
    cat_obj = cat;
    cat_battery_init();
    eyes_closed = false;
    refresh_cat();
    power_timer = lv_timer_create(watch_cat_power, CAT_POWER_SAMPLE_MS, NULL);
    lv_timer_t *timer = lv_timer_create(blink_cat, CAT_OPEN_MS, cat);
    lv_obj_add_event_cb(cat, delete_cat, LV_EVENT_DELETE, timer);
}
K_WORK_DEFINE(cat_attach_work, attach_cat);

static void wait_for_display(struct k_work *work) {
    static unsigned attempts;
    if (zmk_display_is_initialized()) {
        k_work_submit_to_queue(zmk_display_work_q(), &cat_attach_work);
    } else if (++attempts < CAT_BOOT_TRIES) {
        k_work_reschedule(k_work_delayable_from_work(work), K_MSEC(250));
    }
}
K_WORK_DELAYABLE_DEFINE(cat_boot_work, wait_for_display);

static int cat_boot(void) {
    k_work_schedule(&cat_boot_work, K_MSEC(250));
    return 0;
}
SYS_INIT(cat_boot, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

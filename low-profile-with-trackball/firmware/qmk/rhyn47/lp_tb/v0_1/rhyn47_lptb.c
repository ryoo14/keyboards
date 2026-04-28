// Copyright 2025 cormoran707
// Copyright 2026 ryoo (modifications for rhyn47_lptb)
// SPDX-License-Identifier: GPL-2.0-or-later
//
// Modified from the original `dya` keyboard code by cormoran707.

#include "quantum.h"
#include "rhyn47_lptb.h"

static uint8_t mouse_layer        = 0xFF;
static uint8_t scroll_layer       = 0xFF;
static bool    set_scrolling      = false;
static int16_t scroll_h_remaining = 0;
static int16_t scroll_v_remaining = 0;

// functions provided for user

void rhyn47_lptb_set_scroll_layer(uint8_t layer) {
    scroll_layer = layer;
}

void rhyn47_lptb_set_mouse_layer(uint8_t layer) {
    mouse_layer = layer;
}

// keyboard functions

void keyboard_pre_init_kb(void) {
    keyboard_pre_init_user();
}

void keyboard_post_init_kb(void) {
    keyboard_post_init_user();
}

void housekeeping_task_kb(void) {
    housekeeping_task_user();
}

void suspend_power_down_kb(void) {
    suspend_power_down_user();
}

void suspend_wakeup_init_kb(void) {
    suspend_wakeup_init_user();
}

layer_state_t default_layer_state_set_kb(layer_state_t state) {
    if (set_scrolling) {
        set_scrolling = false;
        pointing_device_set_cpi(RHYN47_LPTB_POINTING_CPI);
    }
    return default_layer_state_set_user(state);
}

layer_state_t layer_state_set_kb(layer_state_t state) {
    uint8_t highest_layer = get_highest_layer(state);
    if (highest_layer == scroll_layer) {
        set_scrolling = true;
        pointing_device_set_cpi(RHYN47_LPTB_SCROLL_CPI);
    } else {
        if (set_scrolling) {
            set_scrolling = false;
            pointing_device_set_cpi(RHYN47_LPTB_POINTING_CPI);
        }
    }
    return layer_state_set_user(state);
}

report_mouse_t pointing_device_task_kb(report_mouse_t mouse_report) {
    if (set_scrolling) {
        int16_t h          = (int16_t)(mouse_report.x) + scroll_h_remaining;
        int16_t v          = (int16_t)(mouse_report.y) + scroll_v_remaining;
        mouse_report.h     = h / RHYN47_LPTB_SCROLL_DIVISOR;
        scroll_h_remaining = h % RHYN47_LPTB_SCROLL_DIVISOR;
        mouse_report.v     = v / RHYN47_LPTB_SCROLL_DIVISOR;
        scroll_v_remaining = v % RHYN47_LPTB_SCROLL_DIVISOR;
        mouse_report.x     = 0;
        mouse_report.y     = 0;
    } else {
        scroll_h_remaining = 0;
        scroll_v_remaining = 0;
    }
    return pointing_device_task_user(mouse_report);
}

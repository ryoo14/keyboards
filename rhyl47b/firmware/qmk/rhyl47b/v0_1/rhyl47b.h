// Copyright 2025 cormoran707
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef RHYL47B_H
#define RHYL47B_H

#ifndef RHYL47B_SCROLL_CPI
#    define RHYL47B_SCROLL_CPI 200
#endif

#ifndef RHYL47B_POINTING_CPI
#    define RHYL47B_POINTING_CPI 800
#endif

#ifndef RHYL47B_SCROLL_DIVISOR
#    define RHYL47B_SCROLL_DIVISOR 10
#endif

void rhyl47b_set_scroll_layer(uint8_t layer);
void rhyl47b_set_mouse_layer(uint8_t layer);
#endif

// Copyright 2025 cormoran707
// Copyright 2026 ryoo (modifications for rhyn47-lpt)
// SPDX-License-Identifier: GPL-2.0-or-later
//
// Modified from the original `dya` keyboard code by cormoran707.

#ifndef RHYN47_LPTB_H
#define RHYN47_LPTB_H

#ifndef RHYN47_LPTB_SCROLL_CPI
#    define RHYN47_LPTB_SCROLL_CPI 200
#endif

#ifndef RHYN47_LPTB_POINTING_CPI
#    define RHYN47_LPTB_POINTING_CPI 800
#endif

#ifndef RHYN47_LPTB_SCROLL_DIVISOR
#    define RHYN47_LPTB_SCROLL_DIVISOR 10
#endif

void rhyn47_lptb_set_scroll_layer(uint8_t layer);
void rhyn47_lptb_set_mouse_layer(uint8_t layer);
#endif

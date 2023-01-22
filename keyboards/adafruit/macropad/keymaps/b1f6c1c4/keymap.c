/* Copyright 2022 b1f6c1c4 <b1f6c1c4@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include QMK_KEYBOARD_H

#include "switcher.h"
#include "raw_hid.h"

static int g_perm_tty, g_temp_tty, g_prev_tty, g_vim_tty, g_mod;
static bool g_locked, g_mod_virgin, g_lck_down, g_tty_active;
static uint32_t g_last_mod_down = 0;
static deferred_token g_lck_token;

enum {
    SWKC_1 = KC_1,
    SWKC_2 = KC_2,
    SWKC_3 = KC_3,
    SWKC_4 = KC_4,
    SWKC_5 = KC_5,
    SWKC_6 = KC_6,
    SWKC_7 = KC_7,
    SWKC_8 = KC_8,
    SWKC_LCK = SAFE_RANGE,
    SWKC_BRW,
    SWKC_VIM,
    SWKC_TMP,
    SWKC_ROT,
    SWKC_RLT,
    SWKC_RRT,
};

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [0] = LAYOUT(
            /*    ,     ,*/ SWKC_ROT,
            SWKC_4, SWKC_8, SWKC_TMP,
            SWKC_3, SWKC_7, SWKC_VIM,
            SWKC_2, SWKC_6, SWKC_BRW,
            SWKC_1, SWKC_5, SWKC_LCK
            ),
};

const uint16_t PROGMEM encoder_map[][NUM_ENCODERS][2] = {
    [0] = { ENCODER_CCW_CW(SWKC_RLT, SWKC_RRT) },
};

void keyboard_post_init_user(void) {
    static const char PROGMEM qmk_logo[16 * 32] = {};
    oled_write_raw_P(qmk_logo, sizeof(qmk_logo));
    g_perm_tty = 1, g_temp_tty = g_prev_tty = g_vim_tty = 0;
    g_mod = 0;
    g_locked = g_lck_down = g_tty_active = false;
    rgb_matrix_mode_noeeprom(RGB_MATRIX_SOLID_COLOR);
    rgb_matrix_set_color_all(0, 0, 0);
}

static uint32_t lck_callback(uint32_t trigger_time, void *cb_arg) {
    g_lck_down = false;
    struct switcher_packet pkt;
    pkt.id = SWTCHR_LOCK;
    pkt.payload = SWTCHR_ON;
    raw_hid_send((uint8_t*)&pkt, sizeof(pkt));
    return 0;
}

bool rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max) {
#define RGB_MATRIX_INDICATOR_SET_COLOR_(n, c) \
    RGB_MATRIX_INDICATOR_SET_COLOR(n, c)
#define SWRGB(n, r, g, b, t) do { \
        if (t) { \
            RGB_MATRIX_INDICATOR_SET_COLOR(n, r, g, b); \
        } else { \
            RGB_MATRIX_INDICATOR_SET_COLOR_(n, RGB_BLACK); \
        } \
    } while (false)
    SWRGB(11, 60, 0, 0, g_locked || g_lck_down);
    SWRGB(8 , 100, 27, 35, g_mod == SWKC_BRW);
    SWRGB(5 , 20, 50, 10, g_mod == SWKC_VIM);
    SWRGB(2 , 180, 40, 0, g_mod == SWKC_TMP);
#undef SWRGB
    switch (g_mod) {
        case SWKC_BRW:
            RGB_MATRIX_INDICATOR_SET_COLOR_(0,  RGB_BLACK);
            RGB_MATRIX_INDICATOR_SET_COLOR_(1,  RGB_BLACK);
            RGB_MATRIX_INDICATOR_SET_COLOR(3,   100, 17, 35);
            RGB_MATRIX_INDICATOR_SET_COLOR(4,   100, 17, 35);
            RGB_MATRIX_INDICATOR_SET_COLOR_(6,  RGB_BLACK);
            RGB_MATRIX_INDICATOR_SET_COLOR_(7,  RGB_BLACK);
            RGB_MATRIX_INDICATOR_SET_COLOR(9,   100, 17, 35);
            RGB_MATRIX_INDICATOR_SET_COLOR(10,  100, 17, 35);
            return false;
        case SWKC_VIM:
            RGB_MATRIX_INDICATOR_SET_COLOR_(0,  RGB_BLACK);
            RGB_MATRIX_INDICATOR_SET_COLOR_(1,  RGB_BLACK);
            RGB_MATRIX_INDICATOR_SET_COLOR_(3,  RGB_BLACK);
            RGB_MATRIX_INDICATOR_SET_COLOR(4,   20, 32, 9);
            RGB_MATRIX_INDICATOR_SET_COLOR(6,   20, 32, 9);
            RGB_MATRIX_INDICATOR_SET_COLOR(7,   20, 32, 9);
            RGB_MATRIX_INDICATOR_SET_COLOR_(9,  RGB_BLACK);
            RGB_MATRIX_INDICATOR_SET_COLOR(10,  20, 32, 9);
            return false;
    }
#define SWTTY(n, v) do { \
        if (g_locked) { \
            RGB_MATRIX_INDICATOR_SET_COLOR(n, 60, 0, 0); \
        } else if (g_temp_tty == v && g_perm_tty == v) { \
            RGB_MATRIX_INDICATOR_SET_COLOR(n, 190, 22, 25); \
        } else if (g_temp_tty == v) { \
            RGB_MATRIX_INDICATOR_SET_COLOR(n, 177, 15, 0); \
        } else if (g_perm_tty == v) { \
            RGB_MATRIX_INDICATOR_SET_COLOR(n, 20, 20, 0); \
        } else if (g_vim_tty == v && g_prev_tty == v) { \
            RGB_MATRIX_INDICATOR_SET_COLOR(n, 20, 32, 18); \
        } else if (g_vim_tty == v) { \
            RGB_MATRIX_INDICATOR_SET_COLOR(n, 20, 32, 9); \
        } else if (g_prev_tty == v) { \
            RGB_MATRIX_INDICATOR_SET_COLOR(n, 0, 1, 2); \
        } else { \
            RGB_MATRIX_INDICATOR_SET_COLOR_(n, RGB_BLACK); \
        } \
    } while (false)
    SWTTY(9 , 1);
    SWTTY(6 , 2);
    SWTTY(3 , 3);
    SWTTY(0 , 4);
    SWTTY(10, 5);
    SWTTY(7 , 6);
    SWTTY(4 , 7);
    SWTTY(1 , 8);
#undef SWTTY
#undef RGB_MATRIX_INDICATOR_SET_COLOR_
    return false;
}

void raw_hid_receive(uint8_t *data, uint8_t length) {
    uint8_t payload = data[1];
    switch (data[0]) {
        case SWTCHR_LOCK: g_locked = payload; break;
    }
}

bool process_record_kb(uint16_t keycode, keyrecord_t *record) {
    struct switcher_packet pkt;
    // pkt.id = record->event.pressed ? 114 : 115;
    // pkt.payload = keycode;
    // raw_hid_send((uint8_t*)&pkt, sizeof(pkt));

    int fake, *ptr;
    switch (g_mod) {
        case SWKC_ROT: ptr = &g_prev_tty; break;
        case 0: ptr = &g_perm_tty; break;
        default: ptr = &fake; break;
    }
    switch (keycode) {
        case SWKC_ROT: goto mod;
        case SWKC_BRW: pkt.id = SWTCHR_BROWSER; goto mod;
        case SWKC_VIM: pkt.id = SWTCHR_VIM; goto mod;
        case SWKC_TMP:
            if (g_mod == SWKC_VIM) {
                if (!record->event.pressed) {
                    if (g_vim_tty)
                        g_vim_tty = 0;
                    else
                        g_vim_tty = g_perm_tty;
                }
                return false;
            }
            pkt.id = SWTCHR_TTY;
            goto mod;
        default: break;
mod:
            if (record->event.pressed) { // press
                g_mod = keycode;
                g_mod_virgin = true;
                g_last_mod_down = timer_read32();
                if (keycode == SWKC_TMP) {
                    g_temp_tty = 0;
                    if (g_perm_tty == g_vim_tty) {
                        SEND_STRING(SS_TAP(X_ESC) "" SS_TAP(X_ESC) ":wa");
                    }
                }
            } else if (g_mod_virgin && timer_elapsed32(g_last_mod_down) < 300) { // quick virgin relase
                g_mod = 0;
                // quick tap
                if (keycode == SWKC_TMP) {
                    g_temp_tty = 0;
                    if (g_prev_tty) {
                        pkt.payload = g_prev_tty;
                        g_prev_tty = g_perm_tty;
                        g_perm_tty = pkt.payload;
                        goto send;
                    }
                } else if (keycode != SWKC_ROT) {
                    pkt.payload = SWTCHR_SELF;
                    goto send;
                }
            } else { // slow or not-virgin release
                if (keycode == SWKC_TMP) {
                    if (g_temp_tty && g_tty_active && g_perm_tty != g_temp_tty) {
                        g_prev_tty = g_perm_tty;
                        g_perm_tty = g_temp_tty;
                    }
                    g_temp_tty = 0;
                }
                g_mod = 0;
            }
            return false;
        case SWKC_RLT:
            if (!record->event.pressed) return false;
            pkt.id = SWTCHR_TTY;
            if (--*ptr == 0) *ptr = 8;
            pkt.payload = *ptr;
            if (ptr == &g_perm_tty)
                goto send;
            return false;
        case SWKC_RRT:
            if (!record->event.pressed) return false;
            pkt.id = SWTCHR_TTY;
            if (++*ptr == 9) *ptr = 1;
            pkt.payload = *ptr;
            if (ptr == &g_perm_tty)
                goto send;
            return false;
        case SWKC_LCK:
            if (record->event.pressed) {
                if (!g_lck_down) { // first press
                    g_lck_down = true;
                } else { // second press
                    g_lck_down = false;
                    cancel_deferred_exec(g_lck_token); // do not send LOCK:ON
                }
            } else {
                if (g_lck_down) { // first release
                    g_lck_token = defer_exec(190, lck_callback, NULL);
                    // after 190ms, send LOCK:ON
                } else { // second release
                    SEND_STRING("b1f6c1c4\n");
                }
            }
            return false;
    }

    // Deal with SWKC_1 ~ SWKC_8 depress and release

    g_mod_virgin = false; // not any more
    switch (g_mod) {
        case SWKC_VIM:
            if (!record->event.pressed) return false;
            pkt.id = SWTCHR_VIM;
            switch (keycode) {
                case SWKC_2: pkt.payload = SWTCHR_UP; goto send;
                case SWKC_5: pkt.payload = SWTCHR_LEFT; goto send;
                case SWKC_6: pkt.payload = SWTCHR_DOWN; goto send;
                case SWKC_7: pkt.payload = SWTCHR_RIGHT; goto send;
            }
            return false;
        case SWKC_BRW:
            if (!record->event.pressed) return false;
            pkt.id = SWTCHR_BROWSER;
            switch (keycode) {
                case SWKC_1:
                case SWKC_5: pkt.payload = SWTCHR_LEFT; goto send;
                case SWKC_3:
                case SWKC_7: pkt.payload = SWTCHR_RIGHT; goto send;
            }
            return false;
        case SWKC_TMP:
            pkt.id = SWTCHR_TTY;
            g_tty_active = record->event.pressed;
            switch (keycode) {
#define SWK(v) \
                case SWKC_ ## v: \
                    if (record->event.pressed) pkt.payload = g_temp_tty = v; \
                    else return false; \
                    goto send;
                SWK(1) SWK(2) SWK(3) SWK(4) SWK(5) SWK(6) SWK(7) SWK(8)
#undef SWK
            }
            return false;
        case SWKC_ROT:
            if (!record->event.pressed) return false;
            switch (keycode) {
                case SWKC_1: g_prev_tty = 1; break;
                case SWKC_2: g_prev_tty = 2; break;
                case SWKC_3: g_prev_tty = 3; break;
                case SWKC_4: g_prev_tty = 4; break;
                case SWKC_5: g_prev_tty = 5; break;
                case SWKC_6: g_prev_tty = 6; break;
                case SWKC_7: g_prev_tty = 7; break;
                case SWKC_8: g_prev_tty = 8; break;
                default: return false;
            }
            return false;
        case 0:
            if (!record->event.pressed) return false;
            pkt.id = SWTCHR_TTY;
            switch (keycode) {
                case SWKC_1: pkt.payload = 1; break;
                case SWKC_2: pkt.payload = 2; break;
                case SWKC_3: pkt.payload = 3; break;
                case SWKC_4: pkt.payload = 4; break;
                case SWKC_5: pkt.payload = 5; break;
                case SWKC_6: pkt.payload = 6; break;
                case SWKC_7: pkt.payload = 7; break;
                case SWKC_8: pkt.payload = 8; break;
                default: return false;
            }
            if (g_perm_tty != pkt.payload) {
                g_prev_tty = g_perm_tty;
                g_perm_tty = pkt.payload;
            }
            goto send;
        default:
            return false;
    }

send:
    raw_hid_send((uint8_t*)&pkt, sizeof(pkt));
    return false;
}

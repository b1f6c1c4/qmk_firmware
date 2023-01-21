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

const uint16_t PROGMEM encoder_map[][NUM_ENCODERS][2] = {
  [0] = { ENCODER_CCW_CW(KC_BRID, KC_BRIU) },
};

static void render_qmk_logo(void) {
  static const char PROGMEM qmk_logo[16 * 32] = {
  };

  oled_write_raw_P(qmk_logo, sizeof(qmk_logo));
}

bool oled_task_user(void) {
  render_qmk_logo();
  return true;
}

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
  [0] = LAYOUT(
                              KC_EJCT,
      LCA(KC_F4), LCA(KC_F8), KC_ASST,
      LCA(KC_F3), LCA(KC_F7), KC_CPNL,
      LCA(KC_F2), LCA(KC_F6), KC_MFFD,
      LCA(KC_F1), LCA(KC_F5), KC_MRWD
  )
};

int g_tty;
bool g_z, g_x, g_c, g_v;

void keyboard_post_init_user(void) {
  g_tty = 1;
  g_z = g_x = g_c = g_v = false;
  rgb_matrix_mode_noeeprom(RGB_MATRIX_SOLID_COLOR);
  rgb_matrix_set_color_all(0, 0, 0);
}

bool rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max) {
#define XXX(n, t) do { \
  if (t) { \
    RGB_MATRIX_INDICATOR_SET_COLOR(n, 200, 76, 0); \
  } else { \
    RGB_MATRIX_INDICATOR_SET_COLOR(n, 0, 0, 0); \
  } \
} while (false)

  XXX(9  , g_tty == 1);
  XXX(6  , g_tty == 2);
  XXX(3  , g_tty == 3);
  XXX(0  , g_tty == 4);
  XXX(10 , g_tty == 5);
  XXX(7  , g_tty == 6);
  XXX(4  , g_tty == 7);
  XXX(1  , g_tty == 8);
  XXX(11 , g_z);
  XXX(8  , g_x);
  XXX(5  , g_c);
  XXX(2  , g_v);

  return false;
}

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
  switch (keycode) {
    case LCA(KC_F1): case KC_F1: g_tty = 1; return true;
    case LCA(KC_F2): case KC_F2: g_tty = 2; return true;
    case LCA(KC_F3): case KC_F3: g_tty = 3; return true;
    case LCA(KC_F4): case KC_F4: g_tty = 4; return true;
    case LCA(KC_F5): case KC_F5: g_tty = 5; return true;
    case LCA(KC_F6): case KC_F6: g_tty = 6; return true;
    case LCA(KC_F7): case KC_F7: g_tty = 7; return true;
    case LCA(KC_F8): case KC_F8: g_tty = 8; return true;
    default:
      return true; // Process all other keycodes normally
  }
}

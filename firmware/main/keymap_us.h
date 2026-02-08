#pragma once

#include <stdint.h>

#define MOD_NONE    0x00
#define MOD_LCTRL   0x01
#define MOD_LSHIFT  0x02
#define MOD_LALT    0x04
#define MOD_LGUI    0x08
#define MOD_RCTRL   0x10
#define MOD_RSHIFT  0x20
#define MOD_RALT    0x40
#define MOD_RGUI    0x80

typedef struct {
    uint8_t keycode;
    uint8_t modifier;
} hid_keymap_entry_t;

/* US keyboard layout: ASCII to HID keycode + modifier.
 * Index = ASCII value (0-127). Entries with keycode 0x00 are unmapped. */
static const hid_keymap_entry_t KEYMAP_US[128] = {
    /* 0x00-0x07: Control characters (unmapped) */
    [0x00] = {0x00, MOD_NONE},
    [0x01] = {0x00, MOD_NONE},
    [0x02] = {0x00, MOD_NONE},
    [0x03] = {0x00, MOD_NONE},
    [0x04] = {0x00, MOD_NONE},
    [0x05] = {0x00, MOD_NONE},
    [0x06] = {0x00, MOD_NONE},
    [0x07] = {0x00, MOD_NONE},

    /* 0x08: Backspace, 0x09: Tab, 0x0A: Enter */
    [0x08] = {0x2A, MOD_NONE},  /* Backspace */
    [0x09] = {0x2B, MOD_NONE},  /* Tab */
    [0x0A] = {0x28, MOD_NONE},  /* Enter (Line Feed) */

    /* 0x0B-0x0C */
    [0x0B] = {0x00, MOD_NONE},
    [0x0C] = {0x00, MOD_NONE},

    /* 0x0D: Carriage Return → Enter */
    [0x0D] = {0x28, MOD_NONE},

    /* 0x0E-0x1A */
    [0x0E] = {0x00, MOD_NONE},
    [0x0F] = {0x00, MOD_NONE},
    [0x10] = {0x00, MOD_NONE},
    [0x11] = {0x00, MOD_NONE},
    [0x12] = {0x00, MOD_NONE},
    [0x13] = {0x00, MOD_NONE},
    [0x14] = {0x00, MOD_NONE},
    [0x15] = {0x00, MOD_NONE},
    [0x16] = {0x00, MOD_NONE},
    [0x17] = {0x00, MOD_NONE},
    [0x18] = {0x00, MOD_NONE},
    [0x19] = {0x00, MOD_NONE},
    [0x1A] = {0x00, MOD_NONE},

    /* 0x1B: Escape */
    [0x1B] = {0x29, MOD_NONE},

    /* 0x1C-0x1F */
    [0x1C] = {0x00, MOD_NONE},
    [0x1D] = {0x00, MOD_NONE},
    [0x1E] = {0x00, MOD_NONE},
    [0x1F] = {0x00, MOD_NONE},

    /* 0x20: Space */
    [' ']  = {0x2C, MOD_NONE},

    /* Printable ASCII */
    ['!']  = {0x1E, MOD_LSHIFT},  /* Shift+1 */
    ['"']  = {0x34, MOD_LSHIFT},  /* Shift+' */
    ['#']  = {0x20, MOD_LSHIFT},  /* Shift+3 */
    ['$']  = {0x21, MOD_LSHIFT},  /* Shift+4 */
    ['%']  = {0x22, MOD_LSHIFT},  /* Shift+5 */
    ['&']  = {0x24, MOD_LSHIFT},  /* Shift+7 */
    ['\''] = {0x34, MOD_NONE},    /* ' */
    ['(']  = {0x26, MOD_LSHIFT},  /* Shift+9 */
    [')']  = {0x27, MOD_LSHIFT},  /* Shift+0 */
    ['*']  = {0x25, MOD_LSHIFT},  /* Shift+8 */
    ['+']  = {0x2E, MOD_LSHIFT},  /* Shift+= */
    [',']  = {0x36, MOD_NONE},    /* , */
    ['-']  = {0x2D, MOD_NONE},    /* - */
    ['.']  = {0x37, MOD_NONE},    /* . */
    ['/']  = {0x38, MOD_NONE},    /* / */

    /* Digits */
    ['0']  = {0x27, MOD_NONE},
    ['1']  = {0x1E, MOD_NONE},
    ['2']  = {0x1F, MOD_NONE},
    ['3']  = {0x20, MOD_NONE},
    ['4']  = {0x21, MOD_NONE},
    ['5']  = {0x22, MOD_NONE},
    ['6']  = {0x23, MOD_NONE},
    ['7']  = {0x24, MOD_NONE},
    ['8']  = {0x25, MOD_NONE},
    ['9']  = {0x26, MOD_NONE},

    [':']  = {0x33, MOD_LSHIFT},  /* Shift+; */
    [';']  = {0x33, MOD_NONE},    /* ; */
    ['<']  = {0x36, MOD_LSHIFT},  /* Shift+, */
    ['=']  = {0x2E, MOD_NONE},    /* = */
    ['>']  = {0x37, MOD_LSHIFT},  /* Shift+. */
    ['?']  = {0x38, MOD_LSHIFT},  /* Shift+/ */
    ['@']  = {0x1F, MOD_LSHIFT},  /* Shift+2 */

    /* Uppercase letters */
    ['A']  = {0x04, MOD_LSHIFT},
    ['B']  = {0x05, MOD_LSHIFT},
    ['C']  = {0x06, MOD_LSHIFT},
    ['D']  = {0x07, MOD_LSHIFT},
    ['E']  = {0x08, MOD_LSHIFT},
    ['F']  = {0x09, MOD_LSHIFT},
    ['G']  = {0x0A, MOD_LSHIFT},
    ['H']  = {0x0B, MOD_LSHIFT},
    ['I']  = {0x0C, MOD_LSHIFT},
    ['J']  = {0x0D, MOD_LSHIFT},
    ['K']  = {0x0E, MOD_LSHIFT},
    ['L']  = {0x0F, MOD_LSHIFT},
    ['M']  = {0x10, MOD_LSHIFT},
    ['N']  = {0x11, MOD_LSHIFT},
    ['O']  = {0x12, MOD_LSHIFT},
    ['P']  = {0x13, MOD_LSHIFT},
    ['Q']  = {0x14, MOD_LSHIFT},
    ['R']  = {0x15, MOD_LSHIFT},
    ['S']  = {0x16, MOD_LSHIFT},
    ['T']  = {0x17, MOD_LSHIFT},
    ['U']  = {0x18, MOD_LSHIFT},
    ['V']  = {0x19, MOD_LSHIFT},
    ['W']  = {0x1A, MOD_LSHIFT},
    ['X']  = {0x1B, MOD_LSHIFT},
    ['Y']  = {0x1C, MOD_LSHIFT},
    ['Z']  = {0x1D, MOD_LSHIFT},

    ['[']  = {0x2F, MOD_NONE},    /* [ */
    ['\\'] = {0x31, MOD_NONE},    /* \ */
    [']']  = {0x30, MOD_NONE},    /* ] */
    ['^']  = {0x23, MOD_LSHIFT},  /* Shift+6 */
    ['_']  = {0x2D, MOD_LSHIFT},  /* Shift+- */
    ['`']  = {0x35, MOD_NONE},    /* ` */

    /* Lowercase letters */
    ['a']  = {0x04, MOD_NONE},
    ['b']  = {0x05, MOD_NONE},
    ['c']  = {0x06, MOD_NONE},
    ['d']  = {0x07, MOD_NONE},
    ['e']  = {0x08, MOD_NONE},
    ['f']  = {0x09, MOD_NONE},
    ['g']  = {0x0A, MOD_NONE},
    ['h']  = {0x0B, MOD_NONE},
    ['i']  = {0x0C, MOD_NONE},
    ['j']  = {0x0D, MOD_NONE},
    ['k']  = {0x0E, MOD_NONE},
    ['l']  = {0x0F, MOD_NONE},
    ['m']  = {0x10, MOD_NONE},
    ['n']  = {0x11, MOD_NONE},
    ['o']  = {0x12, MOD_NONE},
    ['p']  = {0x13, MOD_NONE},
    ['q']  = {0x14, MOD_NONE},
    ['r']  = {0x15, MOD_NONE},
    ['s']  = {0x16, MOD_NONE},
    ['t']  = {0x17, MOD_NONE},
    ['u']  = {0x18, MOD_NONE},
    ['v']  = {0x19, MOD_NONE},
    ['w']  = {0x1A, MOD_NONE},
    ['x']  = {0x1B, MOD_NONE},
    ['y']  = {0x1C, MOD_NONE},
    ['z']  = {0x1D, MOD_NONE},

    ['{']  = {0x2F, MOD_LSHIFT},  /* Shift+[ */
    ['|']  = {0x31, MOD_LSHIFT},  /* Shift+\ */
    ['}']  = {0x30, MOD_LSHIFT},  /* Shift+] */
    ['~']  = {0x35, MOD_LSHIFT},  /* Shift+` */

    /* 0x7F: Delete */
    [0x7F] = {0x4C, MOD_NONE},
};

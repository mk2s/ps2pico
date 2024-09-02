/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2024 No0ne (https://github.com/No0ne)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include "pico/stdlib.h"
#include "ps2pico.h"

#include <class/hid/hid.h>

// pull output to high to signal keyboard input, sometime later(in the order of 13ms)
// the first pulse(5.4us) will arrive and we transition the output to the LSB.  Then
// the clock will arrive 18us later, and we advance toward MSB.  This repeates 8 times
// and after that upon the next pulse we output low to signal done.
void kb_output_byte(u8 outbyte) {
    u8 significantbit = 1;

    // output high
    gpio_put(DATAOUT, 1);

    for(u8 i=0; i<8; i++) {
        // wait for clock to go low
        while (gpio_get(CLOCKIN)) {
        }
        // ouput bit - active low
        if (outbyte & significantbit) {
            gpio_put(DATAOUT, 0);
        } else {
            gpio_put(DATAOUT, 1);
        }

        // while the clock is low do nothing
        while (!gpio_get(CLOCKIN)){
        }
        significantbit = significantbit << 1;
    }
    // wait for last pulse
    while (gpio_get(CLOCKIN)) {
    }
    // output low
    gpio_put(DATAOUT, 0);
}

/*
  Otrona key codes bit 7 - not shift key
                   bit 6 - control key
                   bit 5-3 Y coord
                   bit 2-0 X coord

  if given key is not mappable returns INVALID_OTRONA_KEY 0x03
*/
#define INVALID_OTRONA_KEY 0x03

u8 const hid2otrona[] = { /* A */ 0x21, /* B */ 0x22, /* C */ 0x23, /* D */ 0x24, /* E */ 0x25, /* F */ 0x26, /* G */ 0x27,
                          /* H */ 0x28, /* I */ 0x29, /* J */ 0x2a, /* K */ 0x2b, /* L */ 0x2c, /* M */ 0x2d, /* N */ 0x2e, /* O */ 0x2f,
                          /* P */ 0x30, /* Q */ 0x31, /* R */ 0x32, /* S */ 0x33, /* T */ 0x34, /* U */ 0x35, /* V */ 0x36, /* W */ 0x37,
                          /* X */ 0x38, /* Y */ 0x39, /* Z */ 0x3a, 
                          /* 1 */ 0x11, /* 2 */ 0x12, /* 3 */ 0x13, /* 4 */ 0x14, /* 5 */ 0x15, 
                          /* 6 */ 0x16, /* 7 */ 0x17, /* 8 */ 0x18, /* 9 */ 0x19, /* 0 */ 0x10,
                          /* enter */ 0x05, /* esc */ 0x0b, /* bs */ 0x00, /* tab */ 0x01, /* space */ 0x08, /* - */ 0x3e, /* = */ 0x1d,
                          /* [ */ 0x3b, /* ] */ 0x3d, /* backslash */ 0x3c, INVALID_OTRONA_KEY, /* semi */ 0x1b, /* ' */ 0x1a,
                          /* ` */ 0x20, /* , */ 0x1c, /* . */ 0x1e, /* slash */ 0x1f };

u8 toOtrona(u8 key, u8 modifiers){
    u8 otrona = INVALID_OTRONA_KEY;
    if(key >= HID_KEY_A && key <= HID_KEY_SLASH)
    {
        otrona = hid2otrona[key - HID_KEY_A];
    } else if(key == HID_KEY_DELETE) {
        otrona = 0x3f;
    } else if(key == HID_KEY_ARROW_RIGHT) {
        otrona = 0x0d;
    } else if(key == HID_KEY_ARROW_LEFT) {
        otrona = 0x0c;
    } else if(key == HID_KEY_ARROW_DOWN) {
        otrona = 0x0f;
    } else if(key == HID_KEY_ARROW_UP) {
        otrona = 0x0e;
    } else if(key == HID_KEY_END) {
        otrona = 0x02; // mapping end to LF
    } else if(key == HID_KEY_CAPS_LOCK) {
        otrona = 0x07; // lock
    }

    if(otrona == INVALID_OTRONA_KEY) {
        return otrona;
    }

    if(!(modifiers & (KEYBOARD_MODIFIER_LEFTSHIFT | KEYBOARD_MODIFIER_RIGHTSHIFT))) {
        otrona |= 0x80;
    } else {
        // otrona shif 6 is &; shift 7 is *; 8&9 are () and shift 0 is ^
        if (key == HID_KEY_6) {
            otrona = 0x10;
        } else if (key == HID_KEY_7) {
            otrona = 0x16;
        } else if (key == HID_KEY_8) {
            otrona = 0x17;
        } else if (key == HID_KEY_9) {
            otrona = 0x18;
        } else if (key == HID_KEY_0) {
            otrona = 0x19;
        }
    }
    if(modifiers & (KEYBOARD_MODIFIER_LEFTCTRL | KEYBOARD_MODIFIER_RIGHTCTRL)) {
        otrona |= 0x40;
    }
    return otrona;
}

void kb_send_key(u8 key, bool state, u8 modifiers) {
    // only consider keydown events
    if(state) {
        if(modifiers & (KEYBOARD_MODIFIER_LEFTCTRL | KEYBOARD_MODIFIER_RIGHTCTRL) &
        (KEYBOARD_MODIFIER_LEFTCTRL | KEYBOARD_MODIFIER_RIGHTCTRL) && key == HID_KEY_DELETE) {
            printf("TX: CTL-ALT-DEL detected\n", key);
            return;
        }
        printf("TX: %02x %02x\n", key, modifiers);
        u8 otrona = toOtrona(key, modifiers);
        if(otrona != INVALID_OTRONA_KEY) {
            printf("otrona: %02x\n", otrona);
            kb_output_byte(otrona);
        }
    }
}


void kb_reset() {
  // add_alarm_in_ms(50, blink_callback, NULL, false);
}

void kb_init() {
    gpio_init(DATAOUT);
    gpio_set_dir(DATAOUT, GPIO_OUT);
    gpio_put(DATAOUT, 0);

    gpio_init(CLOCKIN);
    gpio_set_function(CLOCKIN, GPIO_FUNC_SIO);
    gpio_set_dir(CLOCKIN, GPIO_IN);
    gpio_pull_up(CLOCKIN);
  // add_alarm_in_ms(1000, reset_detect, NULL, false);
}
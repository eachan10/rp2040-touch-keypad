#include <stdint.h>

#include "pico/stdlib.h"
#include "tusb.h"

#include "usb_descriptors.h"

// ----------------------------------
// for usb hid
// ----------------------------------
void hid_task();
void hid_task_empty();

uint32_t counter = 0;
absolute_time_t last_report_time;
bool has_key = false;


// ----------------------------------
// for touch sensor
// ----------------------------------
#define OUT_PIN_1 22
#define IN_PIN_1 18

#define OUT_PIN_2 13
#define IN_PIN_2 17

uint16_t count;
uint16_t base_count_1;
uint16_t base_count_2;
uint32_t total;
int8_t buffer_1;
int8_t buffer_2;
absolute_time_t last_sample_time;


void capacitive_sense(uint, uint, uint16_t*, uint16_t);

int main() {
    stdio_init_all();
    gpio_init(OUT_PIN_1);
    gpio_set_dir(OUT_PIN_1, GPIO_OUT);
    gpio_put(OUT_PIN_1, 0);
    gpio_init(IN_PIN_1);
    gpio_set_dir(IN_PIN_1, GPIO_IN);
    gpio_set_pulls(IN_PIN_1, 0, 0);

    gpio_init(OUT_PIN_2);
    gpio_set_dir(OUT_PIN_2, GPIO_OUT);
    gpio_put(OUT_PIN_2, 0);
    gpio_init(IN_PIN_2);
    gpio_set_dir(IN_PIN_2, GPIO_IN);
    gpio_set_pulls(IN_PIN_2, 0, 0);


    // configure time to charge without finger touching
    total = 0;
    for (uint8_t i = 16; i > 0; --i) {
        capacitive_sense(OUT_PIN_1, IN_PIN_1, &count, UINT16_MAX);
        total += count;
        sleep_us(20);
    }
    base_count_1 = total / 16 * 1.8;
    total = 0;
    for (uint8_t i = 16; i > 0; --i) {
        capacitive_sense(OUT_PIN_2, IN_PIN_2, &count, UINT16_MAX);
        total += count;
        sleep_us(20);
    }
    base_count_2 = total / 16 * 1.8;

    tusb_init();

    while(1) {
        if (absolute_time_diff_us(last_sample_time, get_absolute_time()) >= 80) {
            capacitive_sense(OUT_PIN_1, IN_PIN_1, &count, base_count_1);
            if (count < base_count_1) {
                if (buffer_1 > 0) --buffer_1;
            } else {
                buffer_1 = 40;
            }
            capacitive_sense(OUT_PIN_2, IN_PIN_2, &count, base_count_2);
            if (count < base_count_2) {
                if (buffer_2 > 0) --buffer_2;
            } else {
                buffer_2 = 40;
            }
            last_sample_time = get_absolute_time();
        }
        tud_task();
        hid_task();
    }
}


void hid_task() {
    if (!tud_hid_ready()) return;
    if (absolute_time_diff_us(last_report_time, get_absolute_time()) < 1000) return;   // report rate of ~1000Hz
    last_report_time = get_absolute_time();
    uint8_t keycode[6] = { 0 };
    if (buffer_1 > 0 || buffer_2 > 0) {
        if (buffer_1 > 0) {
            keycode[0] = HID_KEY_SLASH;
        }
        if (buffer_2 > 0) {
            keycode[1] = HID_KEY_PERIOD;
        }
        has_key = true;
        tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, keycode);
    } else {
        if (has_key) {
            tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, keycode);
        }
        has_key = false;
    }
}




// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen) {

}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize) {

}


void capacitive_sense(uint p_out_pin, uint p_in_pin, uint16_t *count, uint16_t timeout) {
    *count = 0;
    gpio_set_pulls(p_in_pin, 0, 0);  // turn off pull up/down resistors
    gpio_put(p_out_pin, 1);          // start charging the resistor capacitance thing
    while (gpio_get(p_in_pin) == 0 && *count < timeout) {
        ++(*count);
    }
    gpio_put(p_out_pin, 0);          // discharge resistor capacitance
    gpio_set_pulls(p_in_pin, 0, 1);  // pull down resistor enable to quickly discharge resistor capacitance
}
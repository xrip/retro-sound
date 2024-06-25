/* YM2413 Pinout:
                .--\/--.
         GND -- |01  18| <> D1
          D2 <> |02  17| <> D0
          D3 <> |03  16| -- +5V
          D4 <> |04  15| -> RHYTHM OUT
          D5 <> |05  14| -> MELODY OUT
          D6 <> |06  13| <- /RESET
          D7 <> |07  12| <- /CE
         XIN -> |08  11| <- R/W
        XOUT <- |09  10| <- A0
                '------'
 */

/* 595 pin mappings
0-7 DATA BUS

 8 WE/WR
 9 CS 0
10 CS 1
11 A0
12 A1
13 unused
14 unused
15 IC for all chips

8   WE SN76489 #1
9   WE SN76489 #2

10  WE YM2413
11  A0 YM2413
 */

#define SN_1_WE (1 << 8)

#define SAA1_WR (1 << 8)
#define SAA2_WR (1 << 10)
#define SAA_A0 (1 << 9)

#define SN_2_WE (1 << 9)

#define YM_WE (1 << 10)
#define YM_A0 (1 << 11)


#include <cstdio>
#include <cstring>

#include <hardware/structs/vreg_and_chip_reset.h>
#include <pico/multicore.h>
#include <pico/stdlib.h>
#include <hardware/pwm.h>
#include <hardware/structs/clocks.h>
#include <hardware/clocks.h>
#include <hardware/pio.h>
#include "74hc595/74hc595.h"

uint16_t frequencies[] = { 378, 396, 404, 408, 412, 416, 420, 424, 432 };
uint8_t frequency_index = 0;

bool overclock() {
    hw_set_bits(&vreg_and_chip_reset_hw->vreg, VREG_AND_CHIP_RESET_VREG_VSEL_BITS);
    sleep_ms(10);
    return set_sys_clock_khz(frequencies[frequency_index] * 1000, true);
}

#define CLOCK_PIN 29
//#define CLOCK_FREQUENCY (3'579'545)
#define CLOCK_FREQUENCY (3'579'545 * 2)

#define HIGH 1
#define LOW 0

static void clock_init(uint pin) {
    gpio_set_function(pin, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(pin);

    pwm_config c_pwm = pwm_get_default_config();
    pwm_config_set_clkdiv(&c_pwm, clock_get_hz(clk_sys) / (4.0 * CLOCK_FREQUENCY));
    pwm_config_set_wrap(&c_pwm, 3); //MAX PWM value
    pwm_init(slice_num, &c_pwm, true);
    pwm_set_gpio_level(pin, 2);
}

//==============================================================
static inline void ym2413_write_byte(uint8_t addr, uint8_t byte) {
    const bool is_data = (addr & 1);
    write_74hc595(is_data ? YM_A0 : 0 | byte);
//    gpio_put(A0_PIN, addr & 1);
//    gpio_put(WE_PIN, LOW);
//    pio_sm_put(pio, sm, byte);
    busy_wait_us(is_data ? 26 : 5);
    write_74hc595(YM_WE | byte);
//    gpio_put(WE_PIN, HIGH);
}

static inline void sn76489_write_byte(uint8_t byte) {
    write_74hc595(byte | SN_1_WE);
    write_74hc595(byte);
    busy_wait_us(23);
    write_74hc595(byte | SN_1_WE);
}

static inline void saa1099_write_byte(uint8_t chip, uint8_t addr, uint8_t byte) {
    uint16_t is_addr = (addr & 1) == 0 ? SAA_A0 : 0;
    write_74hc595(byte | is_addr | (chip ? SAA1_WR : SAA2_WR)); // опускаем только тот который надо
    busy_wait_us(5);
    write_74hc595(byte | is_addr | SAA1_WR | SAA2_WR); // Возвращаем оба обратно
}

enum chip_type {
    SN76489,
    YM2413,
    YM3812,
    SAA1099,
    YMF262,
    YM2612,
};

/*
 * 76543210 - command byte
 * ||||||||
 * |||||||+-- chip 0 / chip 1
 * ||||||+--- address or data
 * ||||++---- reserved
 * ++++------ 0x0...0xE chip id:
 *
 *            0x0 - SN76489 (Tandy 3 voice)
 *            0x1 - YM2413 (OPLL)
 *            0x2 - YM3812 (OPL2)
 *            0x3 - SAA1099 (Creative Music System / GameBlaster)
 *            0x4 - YMF262 (OPL3)
 *            0x5 - YM2612, YM2203, YM2608, YM2610, YMF288 (OPN2 and OPN-compatible series)
 *            ...
 *            0xf - all chips reset/initialization
 */

#define CHIP(b) (b & 1)
#define TYPE(b) (0 == (b & 2))

int __time_critical_func(main)() {
    //overclock();
    stdio_usb_init();
    clock_init(CLOCK_PIN);
    init_74hc595();

//    write_74hc595(SAA1_WR);
//    write_74hc595(SAA2_WR);

    bool is_data_byte = 0;
    uint8_t command = 0;

    while (true) {
        int data = getchar_timeout_us(1);
        if (PICO_ERROR_TIMEOUT != data) {
            if (is_data_byte) {
                switch (command >> 4) {
                    case SN76489:
                        sn76489_write_byte(/* CHIP(command), */ data);
                        break;
                    case YM2413:
                        ym2413_write_byte(TYPE(command), data);
                        break;
                    case SAA1099:
//                        if(CHIP(command) == 0)
                        saa1099_write_byte(CHIP(command), TYPE(command), data);
                        break;
                    case 0xf:
                    default:
                        break;
                }
            }
            //sn76489_write_byte(byte & 0xFF);
            command = data;
            is_data_byte ^= 1;
        }
    }
}


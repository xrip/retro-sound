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

bool overclock() {
    hw_set_bits(&vreg_and_chip_reset_hw->vreg, VREG_AND_CHIP_RESET_VREG_VSEL_BITS);
    sleep_ms(10);
    return set_sys_clock_khz(252 * 1000, true);
}

#define CLOCK_PIN 29
#define CLOCK_FREQUENCY (3'579'545 * 2)

#define CLOCK_PIN2 23
#define CLOCK_FREQUENCY2 (3'579'545)

#define SN_1_WE (1 << 12)

#define SAA_1_WR (1 << 8)
#define SAA_2_WR (1 << 10)
#define A0 (1 << 9)

#define YM_WE (1 << 11)

static void clock_init(uint pin, uint32_t frequency) {
    gpio_set_function(pin, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(pin);

    pwm_config c_pwm = pwm_get_default_config();
    pwm_config_set_clkdiv(&c_pwm, clock_get_hz(clk_sys) / (4.0 * frequency));
    pwm_config_set_wrap(&c_pwm, 3); // MAX PWM value
    pwm_init(slice_num, &c_pwm, true);
    pwm_set_gpio_level(pin, 2);
}

static uint16_t control_bits = 0;
#define LOW(x) (control_bits &= ~(x))
#define HIGH(x) (control_bits |= (x))

// SN76489
static inline void sn76489_write(uint8_t byte) {
    write_74hc595(byte | HIGH(SN_1_WE));
    write_74hc595(byte | LOW(SN_1_WE));
    busy_wait_us(23);
    write_74hc595(byte | HIGH(SN_1_WE));
}


// YM2413
static inline void ym2413_write(uint8_t addr, uint8_t byte) {
    const uint16_t a0 = addr ? A0 : 0;
    write_74hc595(byte | a0 | LOW(YM_WE));
    busy_wait_us(4);
    write_74hc595(byte | a0 | HIGH(YM_WE));
    busy_wait_us(a0 ? 30 : 5);
}

// SAA1099
static inline void saa1099_write(uint8_t chip, uint8_t addr, uint8_t byte) {
    const uint16_t a0 = addr ? A0 : 0;
    write_74hc595(byte | a0 | LOW(chip ? SAA_2_WR : SAA_1_WR)); // опускаем только тот который надо
    busy_wait_us(5);
    write_74hc595(byte | a0 | HIGH(SAA_1_WR | SAA_2_WR)); // Возвращаем оба обратно
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

#define CHIP(b) (b >> 4)
#define CHIPN(b) (b & 1)
#define TYPE(b) (b & 2)

int __time_critical_func(main)() {
    overclock();

    stdio_usb_init();

    clock_init(CLOCK_PIN, CLOCK_FREQUENCY);
    clock_init(CLOCK_PIN2, CLOCK_FREQUENCY2);

    init_74hc595();

    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

    write_74hc595(HIGH(YM_WE | SAA_1_WR | SAA_2_WR));

    bool is_data_byte = false;
    uint8_t command = 0;

    while (true) {
        int data = getchar_timeout_us(1);
        if (PICO_ERROR_TIMEOUT != data) {
            if (is_data_byte) {
                gpio_put(PICO_DEFAULT_LED_PIN, TYPE(command));

                switch (CHIP(command)) {
                    case SN76489: /* TODO: GameGear channel mapping */
                        sn76489_write(data);
                        break;
                    case YM2413:
                        ym2413_write(TYPE(command), data);
                        break;
                    case SAA1099:
                        saa1099_write(CHIPN(command), TYPE(command), data);
                        break;

                    case YM3812:
                    case YMF262:
                    case YM2612:
                    // Reset
                    case 0xf:
                    default:
                        /* TODO Global Reset /IC for YM chips */
                        break;
                }
            }

            command = data;
            is_data_byte ^= 1;
        }
    }
}


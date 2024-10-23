#include <cstdio>

#include <hardware/structs/vreg_and_chip_reset.h>
#include <pico/stdlib.h>
#include <hardware/pwm.h>
#include <hardware/structs/clocks.h>
#include <hardware/clocks.h>
#include <hardware/pio.h>
#include <pico/multicore.h>
#include <pico/sem.h>

#include "74hc595.h"

#define SOUND_FREQUENCY 49716
extern "C" {
#include "audio.h"
    }
i2s_config_t i2s_config;
static int16_t samples[2][888*2] = { 0 };
static int active_buffer = 0;
static int sample_index = 0;

static inline bool overclock() {
    hw_set_bits(&vreg_and_chip_reset_hw->vreg, VREG_AND_CHIP_RESET_VREG_VSEL_BITS);
    sleep_ms(10);
    return set_sys_clock_khz(378 * KHZ, true);
}

#define RESET_KEY_PIN (24)
#define TEST_KEY_PIN (14)

#define BASE_CLOCK_FREQUENCY (3'579'545)

#define CLOCK_PIN 23
#define CLOCK_FREQUENCY (BASE_CLOCK_FREQUENCY * 2)

#define CLOCK_PIN2 29
#define CLOCK_FREQUENCY2 (BASE_CLOCK_FREQUENCY * 1)

#define A0 (1 << 8)
#define A1 (1 << 9)

#define IC (1 << 10)

#define SN_1_CS (1 << 11)

#define SAA_1_CS (1 << 12)
#define SAA_2_CS (1 << 13)
#define OPL2 (1 << 14)
#define OPL3 (1 << 15)

#if SN76489_REVERSED
// Если мы перепутаем пины
static const uint8_t  __aligned(4) reversed[] = {
        0x00, 0x80, 0x40, 0xC0, 0x20, 0xA0, 0x60, 0xE0, 0x10, 0x90, 0x50, 0xD0, 0x30, 0xB0, 0x70, 0xF0,
        0x08, 0x88, 0x48, 0xC8, 0x28, 0xA8, 0x68, 0xE8, 0x18, 0x98, 0x58, 0xD8, 0x38, 0xB8, 0x78, 0xF8,
        0x04, 0x84, 0x44, 0xC4, 0x24, 0xA4, 0x64, 0xE4, 0x14, 0x94, 0x54, 0xD4, 0x34, 0xB4, 0x74, 0xF4,
        0x0C, 0x8C, 0x4C, 0xCC, 0x2C, 0xAC, 0x6C, 0xEC, 0x1C, 0x9C, 0x5C, 0xDC, 0x3C, 0xBC, 0x7C, 0xFC,
        0x02, 0x82, 0x42, 0xC2, 0x22, 0xA2, 0x62, 0xE2, 0x12, 0x92, 0x52, 0xD2, 0x32, 0xB2, 0x72, 0xF2,
        0x0A, 0x8A, 0x4A, 0xCA, 0x2A, 0xAA, 0x6A, 0xEA, 0x1A, 0x9A, 0x5A, 0xDA, 0x3A, 0xBA, 0x7A, 0xFA,
        0x06, 0x86, 0x46, 0xC6, 0x26, 0xA6, 0x66, 0xE6, 0x16, 0x96, 0x56, 0xD6, 0x36, 0xB6, 0x76, 0xF6,
        0x0E, 0x8E, 0x4E, 0xCE, 0x2E, 0xAE, 0x6E, 0xEE, 0x1E, 0x9E, 0x5E, 0xDE, 0x3E, 0xBE, 0x7E, 0xFE,
        0x01, 0x81, 0x41, 0xC1, 0x21, 0xA1, 0x61, 0xE1, 0x11, 0x91, 0x51, 0xD1, 0x31, 0xB1, 0x71, 0xF1,
        0x09, 0x89, 0x49, 0xC9, 0x29, 0xA9, 0x69, 0xE9, 0x19, 0x99, 0x59, 0xD9, 0x39, 0xB9, 0x79, 0xF9,
        0x05, 0x85, 0x45, 0xC5, 0x25, 0xA5, 0x65, 0xE5, 0x15, 0x95, 0x55, 0xD5, 0x35, 0xB5, 0x75, 0xF5,
        0x0D, 0x8D, 0x4D, 0xCD, 0x2D, 0xAD, 0x6D, 0xED, 0x1D, 0x9D, 0x5D, 0xDD, 0x3D, 0xBD, 0x7D, 0xFD,
        0x03, 0x83, 0x43, 0xC3, 0x23, 0xA3, 0x63, 0xE3, 0x13, 0x93, 0x53, 0xD3, 0x33, 0xB3, 0x73, 0xF3,
        0x0B, 0x8B, 0x4B, 0xCB, 0x2B, 0xAB, 0x6B, 0xEB, 0x1B, 0x9B, 0x5B, 0xDB, 0x3B, 0xBB, 0x7B, 0xFB,
        0x07, 0x87, 0x47, 0xC7, 0x27, 0xA7, 0x67, 0xE7, 0x17, 0x97, 0x57, 0xD7, 0x37, 0xB7, 0x77, 0xF7,
        0x0F, 0x8F, 0x4F, 0xCF, 0x2F, 0xAF, 0x6F, 0xEF, 0x1F, 0x9F, 0x5F, 0xDF, 0x3F, 0xBF, 0x7F, 0xFF
};
#endif

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
static inline void SN76489_write(uint8_t byte) {
#if SN76489_REVERSED
    byte = reversed[byte];
#endif
    write_74hc595(byte | LOW(SN_1_CS), 20);
    write_74hc595(byte | HIGH(SN_1_CS), 0);
}


// YM2413
static inline void YM2413_write(uint8_t addr, uint8_t byte) {
    const uint16_t a0 = addr ? A0 : 0;
    write_74hc595(byte | a0 | LOW(OPL2), 4);
    write_74hc595(byte | a0 | HIGH(OPL2), a0 ? 30 : 5);
}

// SAA1099
static inline void SAA1099_write(uint8_t addr, uint8_t chip, uint8_t byte) {
    const uint16_t a0 = addr ? A0 : 0;
    const uint16_t cs = chip ? SAA_2_CS : SAA_1_CS;

    write_74hc595(byte | a0 | LOW(cs), 5);
    write_74hc595(byte | a0 | HIGH(cs), 0);
}

// YM3812 / YM2413
static inline void OPL2_write_byte(uint16_t addr, uint16_t register_set, uint8_t byte) {
    const uint16_t a0 = addr ? A0 : 0;
    const uint16_t a1 = register_set ? A1 : 0;

    write_74hc595(byte | a0 | a1 | LOW(OPL2), 5);
    write_74hc595(byte | a0 | a1 | HIGH(OPL2), 30);
}

// YM3812 / YMF262
static inline void OPL3_write_byte(uint16_t addr, uint16_t register_set, uint8_t byte) {
    const uint16_t a0 = addr ? A0 : 0;
    const uint16_t a1 = register_set ? A1 : 0;

    write_74hc595(byte | a0 | a1 | LOW(OPL3), 5);
    write_74hc595(byte | a0 | a1 | HIGH(OPL3), 0);
}

enum chip_type {
    SN76489,
    YM2413,
    YM3812,
    SAA1099,
    YMF262,
    YM2612,
    DAC = 0xe,
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

void static inline reset_chips() {
    control_bits = 0;
    write_74hc595(HIGH(SN_1_CS | OPL2 | SAA_1_CS | SAA_2_CS | OPL3), 0);
    write_74hc595(HIGH(IC), 0);
    sleep_ms(10);
    write_74hc595(LOW(IC), 0);
    sleep_ms(100);
    write_74hc595(HIGH(IC), 0);
    sleep_ms(10);

    // Mute SN76489
    SN76489_write(0x9F);
    sleep_ms(10);
    SN76489_write(0xBF);
    sleep_ms(10);
    SN76489_write(0xDF);
    sleep_ms(10);
    SN76489_write(0xFF);
    sleep_ms(10);

    for (int i = 0; i < 6; i++) {
        sleep_ms(23);
        gpio_put(PICO_DEFAULT_LED_PIN, true);
        sleep_ms(23);
        gpio_put(PICO_DEFAULT_LED_PIN, false);
    }
}
semaphore vga_start_semaphore;
volatile int16_t dac_sample = 0;
void __time_critical_func() second_core() {

    uint64_t tick = time_us_64();
    uint64_t last_sound_tick = tick;

    sem_acquire_blocking(&vga_start_semaphore);
    i2s_config = i2s_get_default_config();
    i2s_config.sample_freq = SOUND_FREQUENCY;
    i2s_config.dma_trans_count = 444; // 60 FPS
    i2s_volume(&i2s_config, 0);
    i2s_init(&i2s_config);
    sleep_ms(100);
    while (true) {
        // Sound frequency 44100
        if (tick >= last_sound_tick + (1000000 / SOUND_FREQUENCY)) {
            samples[active_buffer][sample_index * 2] = dac_sample;
            samples[active_buffer][sample_index * 2 + 1] = dac_sample;
            if (sample_index++ >= i2s_config.dma_trans_count) {
                sample_index = 0;
                i2s_dma_write(&i2s_config, samples[active_buffer]);
                active_buffer ^= 1;
                // gpio_put(PICO_DEFAULT_LED_PIN, active_buffer);
            }

            last_sound_tick = tick;
        }

        tick = time_us_64();
        tight_loop_contents();
    }
}

int  __time_critical_func() main() {
    overclock();

    stdio_usb_init();

    sem_init(&vga_start_semaphore, 0, 1);
    multicore_launch_core1(second_core);
    sem_release(&vga_start_semaphore);

    clock_init(CLOCK_PIN, CLOCK_FREQUENCY);
    clock_init(CLOCK_PIN2, CLOCK_FREQUENCY2);

    gpio_init(RESET_KEY_PIN);
    gpio_set_dir(RESET_KEY_PIN, GPIO_IN);
    gpio_pull_up(RESET_KEY_PIN);

    init_74hc595();

    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

    gpio_init(TEST_KEY_PIN);
    gpio_set_dir(TEST_KEY_PIN, GPIO_OUT);

    reset_chips();
    int is_data_byte = false;
    int current_clock = 0;
    uint8_t command = 0;


    while (0) {
        write_74hc595(0b0101, 0);
        sleep_ms(1000);
        write_74hc595(0b1010, 0);
        sleep_ms(1000);
    }

    while (true) {
        gpio_put(TEST_KEY_PIN, false);

        if (!gpio_get(RESET_KEY_PIN)) { reset_chips(); }
        int data = getchar_timeout_us(500);

        if (PICO_ERROR_TIMEOUT != data) {
            gpio_put(TEST_KEY_PIN, true);

            if (is_data_byte) {
                gpio_put(PICO_DEFAULT_LED_PIN, TYPE(command));

                switch (CHIP(command)) {
                    case SN76489: /* TODO: GameGear channel mapping */
                        if (current_clock != 0) {
                            clock_init(CLOCK_PIN2, BASE_CLOCK_FREQUENCY);
                            current_clock = 0;
                        }
                        SN76489_write(data);
                        break;

                    case YM2413:
                        YM2413_write(TYPE(command), data);
                        break;

                    case YMF262:
                        if (current_clock != 1) {
                            clock_init(CLOCK_PIN2, 4 * BASE_CLOCK_FREQUENCY);
                            current_clock = 1;
                        }
                        OPL3_write_byte(TYPE(command), CHIPN(command), data);
                        break;
                    case YM3812:
                    case YM2612:
                        if (current_clock != 0) {
                            clock_init(CLOCK_PIN2, BASE_CLOCK_FREQUENCY);
                            current_clock = 0;
                        }
                        OPL2_write_byte(TYPE(command), CHIPN(command), data);
                        break;

                    case SAA1099:
                        SAA1099_write(TYPE(command), CHIPN(command), data);
                        break;

                    case DAC: {
                        static uint8_t sample;
                        if (CHIPN(command)) {
                            dac_sample = (int16_t)(sample << 8) | (int16_t)(data & 0xff);
                        } else {
                            sample = data & 0xff;
                        }


                        break;
                    }
                    case 0xf:
                        if (command & 0b1000) {
                            const uint8_t clock_multiplier = (command & 0b11) + 1;
                            clock_init(command & 0b100 ? CLOCK_PIN : CLOCK_PIN2,
                                       BASE_CLOCK_FREQUENCY * clock_multiplier);

                        }

                        reset_chips();
                        break;
                }
            }

            command = data;
            is_data_byte ^= 1;
        }
    }
}


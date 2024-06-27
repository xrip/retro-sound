#include <cstdio>
#include <cstring>

#include <hardware/structs/vreg_and_chip_reset.h>
#include <pico/multicore.h>
#include <pico/stdlib.h>
#include <hardware/pwm.h>
#include <hardware/structs/clocks.h>
#include <hardware/clocks.h>
#include <hardware/pio.h>

#include "audio.h"

bool overclock() {
    hw_set_bits(&vreg_and_chip_reset_hw->vreg, VREG_AND_CHIP_RESET_VREG_VSEL_BITS);
    sleep_ms(10);
    return set_sys_clock_khz(252 * 1000, true);
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

#define PWM_PIN0 (26)
#define PWM_PIN1 (27)
#define BEEPER_PIN (28)

#define SOUND_FREQUENCY 44100

i2s_config_t i2s_config = i2s_get_default_config();
static int16_t samples[2][888*2] = { 0 };
static int active_buffer = 0;
static int sample_index = 0;

extern "C" void cms_samples(int16_t* output);
extern "C" int16_t sn76489_sample();
extern "C" void sn76489_reset();

semaphore vga_start_semaphore;
bool started = false;

void __time_critical_func(second_core)() {
    sn76489_reset();

    uint64_t tick = time_us_64();
    uint64_t last_timer_tick = tick, last_cursor_blink = tick, last_sound_tick = tick, last_dss_tick = tick;
    sem_acquire_blocking(&vga_start_semaphore);
    while (true) {
	// Sound frequency 44100
        if (tick >= last_sound_tick + (1000000 / SOUND_FREQUENCY)) {
            int sample = 0;

            sample += sn76489_sample() << 2;

            samples[active_buffer][sample_index * 2] = sample;
            samples[active_buffer][sample_index * 2 + 1] = sample;

            //cms_samples(&samples[active_buffer][sample_index * 2]);

            if (sample_index++ >= i2s_config.dma_trans_count) {
                sample_index = 0;
                i2s_dma_write(&i2s_config, samples[active_buffer]);
                active_buffer ^= 1;
            }

            last_sound_tick = tick;
        }

        tick = time_us_64();
        tight_loop_contents();
    }
}

extern "C" void cms_out(uint16_t addr, uint16_t value);
extern "C" void sn76489_out(uint16_t value);

static inline void saa1099_write(uint8_t chip, uint8_t addr, uint8_t byte) {
    static uint16_t latch_register;
    if (addr == 0) {
        latch_register = byte;
    }


}

int __time_critical_func(main)() {
    overclock();

    stdio_init_all();

    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);


    for (int i = 0; i < 6; i++) {
        sleep_ms(23);
        gpio_put(PICO_DEFAULT_LED_PIN, true);
        sleep_ms(23);
        gpio_put(PICO_DEFAULT_LED_PIN, false);
    }

    i2s_config.sample_freq = SOUND_FREQUENCY;
    i2s_config.dma_trans_count = SOUND_FREQUENCY / 60;
    i2s_volume(&i2s_config, 0);
    i2s_init(&i2s_config);
    sleep_ms(100);

    sem_init(&vga_start_semaphore, 0, 1);
    multicore_launch_core1(second_core);
    sem_release(&vga_start_semaphore);


    sleep_ms(100);


    bool is_data_byte = false;
    uint8_t command = 0;

    while (true) {
        int data = getchar_timeout_us(1);
        if (PICO_ERROR_TIMEOUT != data) {
            gpio_put(PICO_DEFAULT_LED_PIN, is_data_byte);
            started = true;
            sn76489_out(data);
            is_data_byte = !is_data_byte;
        }
    }
}


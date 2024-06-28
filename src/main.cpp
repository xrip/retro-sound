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
extern "C" {

#include "emulators/ym2413.h"
}

bool overclock() {
    hw_set_bits(&vreg_and_chip_reset_hw->vreg, VREG_AND_CHIP_RESET_VREG_VSEL_BITS);
    sleep_ms(10);
    return set_sys_clock_khz(378 * 1000, true);
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

extern "C" void adlib_init(uint32_t samplerate);
extern "C" void adlib_write(uintptr_t idx, uint8_t val);
extern "C" void adlib_getsample(int16_t* sndptr, intptr_t numsamples);

semaphore vga_start_semaphore;
bool started = false;

void __time_critical_func(second_core)() {

    sn76489_reset();
    YM2413Init(1, 3'579'545, SOUND_FREQUENCY);
    YM2413ResetChip(0);

    adlib_init(SOUND_FREQUENCY);

    uint64_t tick = time_us_64();
    uint64_t last_sound_tick = tick;

    sem_acquire_blocking(&vga_start_semaphore);
    while (true) {
	// Sound frequency 44100
        if (tick >= last_sound_tick + (1000000 / SOUND_FREQUENCY)) {
            int16_t *lr[2];
            lr[0] =  &samples[active_buffer][sample_index * 2];
            lr[1] =  &samples[active_buffer][sample_index * 2 + 1];

            YM2413UpdateOne(0, lr, 1);

            int16_t sample = sn76489_sample();

            samples[active_buffer][sample_index * 2] += sample;
            samples[active_buffer][sample_index * 2 + 1] += sample;

            cms_samples(&samples[active_buffer][sample_index * 2]);

            adlib_getsample(&sample, 1);

            samples[active_buffer][sample_index * 2] += sample;
            samples[active_buffer][sample_index * 2 + 1] += sample;

            if (sample_index++ >= i2s_config.dma_trans_count) {
                //adlib_getsample(samples[active_buffer], sample_index);
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
    cms_out(0x220+(chip*2)+(addr == 2), byte);
}

int __time_critical_func(main)() {
    overclock();

    stdio_init_all();

    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

    i2s_config.sample_freq = SOUND_FREQUENCY;
    i2s_config.dma_trans_count = SOUND_FREQUENCY / 60; // 60 FPS
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
            if (is_data_byte) {
                gpio_put(PICO_DEFAULT_LED_PIN, TYPE(command));

                switch (CHIP(command)) {
                    case SN76489: /* TODO: GameGear channel mapping */
                        sn76489_out(data & 0xff);
                        break;
                    case YM2413:
                        YM2413Write(0, TYPE(command) == 2, data);
                        //ym2413_write(TYPE(command), data);
                        break;
                    case SAA1099:
                        saa1099_write(CHIPN(command), TYPE(command), data);
                        break;

                    case YM3812: {
                        static uint8_t  latched_register;
                        if (!TYPE(command)) {
                            latched_register = data & 0xff;
                        } else {
                            adlib_write(latched_register, data & 0xff);
                        }
                        break;
                    }
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


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

8   WE SN76489 #1
9   WE SN76489 #2

10  WE YM2413
11  A0 YM2413
 */

#define SN_1_WE (1 << 8)
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
#include "74hc595/AY_PIO_595.h"

uint16_t frequencies[] = { 272, 396, 404, 408, 412, 416, 420, 424, 432 };
uint8_t frequency_index = 0;

bool overclock() {
    hw_set_bits(&vreg_and_chip_reset_hw->vreg, VREG_AND_CHIP_RESET_VREG_VSEL_BITS);
    sleep_ms(10);
    return set_sys_clock_khz(frequencies[frequency_index] * 1000, true);
}

#define DATA_START_PIN 2
#define A0_PIN 15
#define WE_PIN 23
#define CLOCK_PIN 21

#define CLOCK_FREQUENCY (3'579'545)

#define HIGH 1
#define LOW 0

static void clock_init(uint pin) {
    gpio_set_function(pin, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(pin);

    pwm_config c_pwm = pwm_get_default_config();
    pwm_config_set_clkdiv(&c_pwm, clock_get_hz(clk_sys) / (1.0 * (4433000 / 1)));
    //pwm_config_set_clkdiv(&c_pwm, clock_get_hz(clk_sys) / (4.0 * CLOCK_FREQUENCY));
    pwm_config_set_wrap(&c_pwm, 3); //MAX PWM value
    pwm_init(slice_num, &c_pwm, true);
    pwm_set_gpio_level(pin, 2);
}

PIO pio = pio1;
uint sm = pio_claim_unused_sm(pio, true);

// The function to set up the PIO and load the program
void ym2413_init(uint pin_base) {
    const uint16_t out_instr = pio_encode_out(pio_pins, 8);
    const struct pio_program chip_programm = {
            .instructions = &out_instr,
            .length = 1,
            .origin = -1,
    };

    const uint offset = pio_add_program(pio, &chip_programm);
//    pio_gpio_init(pio, WE);

    for (int i = 0; i < 8; i++) {
        pio_gpio_init(pio, pin_base + i);

    }

    pio_sm_set_consecutive_pindirs(pio, sm, pin_base, 8, true);
//    pio_sm_set_consecutive_pindirs(pio, sm, WE, 1, true);

    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset + 0, offset + (chip_programm.length - 1));

    sm_config_set_out_pins(&c, pin_base, 8);
//    sm_config_set_sideset_pins(&c, WE);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);
    sm_config_set_out_shift(&c, true, true, 8);

    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);
/*
    gpio_init(IC_PIN);
    gpio_set_dir(IC_PIN, GPIO_OUT);
    gpio_put(IC_PIN, HIGH);
    sleep_ms(1);
    gpio_put(IC_PIN, LOW);
    sleep_ms(1);
    gpio_put(IC_PIN, HIGH);
*/
    gpio_init(A0_PIN);
    gpio_set_dir(A0_PIN, GPIO_OUT);

    gpio_init(WE_PIN);
    gpio_set_dir(WE_PIN, GPIO_OUT);
    gpio_put(WE_PIN, HIGH);
/*
    gpio_init(CS_PIN);
    gpio_set_dir(CS_PIN, GPIO_OUT);
    gpio_put(CS_PIN, HIGH);
*/
}

//==============================================================
static inline void ym2413_write_byte(uint8_t addr, uint8_t byte) {
    gpio_put(A0_PIN, addr & 1);
    gpio_put(WE_PIN, LOW);
    pio_sm_put(pio, sm, byte);
    busy_wait_us(1 == (addr & 1) ? 26 : 5);
    gpio_put(WE_PIN, HIGH);
}

static inline void sn76489_write_byte(uint8_t byte) {
    SendAY(SN_1_WE | byte);
    SendAY(SN_1_WE);

    gpio_put(WE_PIN, HIGH);
    pio_sm_put(pio, sm, byte);
    gpio_put(WE_PIN, LOW);
    busy_wait_us(23);
    gpio_put(WE_PIN, HIGH);
}

// Define AY-3-8910 register addresses
#define AY_REG_TONE_A_LOW   0x00
#define AY_REG_TONE_A_HIGH  0x01
#define AY_REG_TONE_B_LOW   0x02
#define AY_REG_TONE_B_HIGH  0x03
#define AY_REG_TONE_C_LOW   0x04
#define AY_REG_TONE_C_HIGH  0x05
#define AY_REG_VOL_A        0x08
#define AY_REG_VOL_B        0x09
#define AY_REG_VOL_C        0x0A

typedef struct {
    uint8_t reg;
    uint8_t value;
} AYCommand;

AYCommand convert_sn76489_to_ay(uint8_t sn_data_byte, uint8_t data_value) {
    AYCommand ay_cmd;
    uint8_t command_type = (sn_data_byte & 0xF0) >> 4;
    uint8_t channel = (sn_data_byte & 0x60) >> 5;
    uint8_t data = sn_data_byte & 0x0F;

    switch (command_type) {
        case 0x8:  // Tone frequency, low 4 bits
            switch (channel) {
                case 0:
                    ay_cmd.reg = AY_REG_TONE_A_LOW;
                    break;
                case 1:
                    ay_cmd.reg = AY_REG_TONE_B_LOW;
                    break;
                case 2:
                    ay_cmd.reg = AY_REG_TONE_C_LOW;
                    break;
                default:
                    ay_cmd.reg = 0xFF;
                    ay_cmd.value = 0xFF;
                    return ay_cmd;
            }
            ay_cmd.value = data_value & 0x0F;
            break;

        case 0x9:  // Tone frequency, high 6 bits
            switch (channel) {
                case 0:
                    ay_cmd.reg = AY_REG_TONE_A_HIGH;
                    break;
                case 1:
                    ay_cmd.reg = AY_REG_TONE_B_HIGH;
                    break;
                case 2:
                    ay_cmd.reg = AY_REG_TONE_C_HIGH;
                    break;
                default:
                    ay_cmd.reg = 0xFF;
                    ay_cmd.value = 0xFF;
                    return ay_cmd;
            }
            ay_cmd.value = data_value & 0x3F;
            break;

        case 0xA:  // Volume
            switch (channel) {
                case 0:
                    ay_cmd.reg = AY_REG_VOL_A;
                    break;
                case 1:
                    ay_cmd.reg = AY_REG_VOL_B;
                    break;
                case 2:
                    ay_cmd.reg = AY_REG_VOL_C;
                    break;
                default:
                    ay_cmd.reg = 0xFF;
                    ay_cmd.value = 0xFF;
                    return ay_cmd;
            }
            ay_cmd.value = data_value & 0x0F;
            break;

        default:
            // Unsupported command, return a default command
            ay_cmd.reg = 0xFF;
            ay_cmd.value = 0xFF;
            break;
    }

    return ay_cmd;
}


int __time_critical_func(main)() {
    //overclock();
    stdio_usb_init();
    clock_init(CLOCK_PIN);
    InitAY();
    sleep_ms(1000);
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    bool addr_or_data = 0;
    uint8_t reg;

    while (true) {
        int byte = getchar_timeout_us(1);
        if (PICO_ERROR_TIMEOUT != byte) {
            if (addr_or_data == 0) {
                reg = byte;
            } else {
                AYCommand cmd = convert_sn76489_to_ay(reg, byte);
                WriteAY(cmd.reg, cmd.value);
                //ym2413_write_byte(reg, byte);
            }
            /*
            SendAY(byte | SN_1_WE);
            SendAY(byte);
            busy_wait_us(23);
            SendAY(byte | SN_1_WE);
             */
                //ym2413_write_byte(reg, byte);
            addr_or_data ^= 1;
        }
    }
/*    while(1) {
        gpio_put(PICO_DEFAULT_LED_PIN, HIGH);
        SendAY(SN_1_WE | 0b10);
        sleep_ms(1000);
        gpio_put(PICO_DEFAULT_LED_PIN, LOW);
        SendAY(0);
        sleep_ms(1000);
    }*/
}
int __time_critical_func(main1)() {
    overclock();
    stdio_usb_init();

    clock_init(CLOCK_PIN);
    ym2413_init(DATA_START_PIN);

    bool addr_or_data = 0;
    uint8_t reg;

    while (true) {
        int byte = getchar_timeout_us(1);
        if (PICO_ERROR_TIMEOUT != byte) {
            if (addr_or_data == 0) {
                reg = byte & 0x1;
            } else {
                ym2413_write_byte(reg, byte);
            }
            addr_or_data ^= 1;
        }
    }
}

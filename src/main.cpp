#include <cstdio>
#include <cstring>
#include <hardware/flash.h>
#include <hardware/structs/vreg_and_chip_reset.h>
#include <pico/multicore.h>
#include <pico/stdlib.h>
#include <hardware/pwm.h>
#include <hardware/structs/clocks.h>
#include <hardware/clocks.h>
#include <hardware/pio.h>

uint16_t frequencies[] = { 272, 396, 404, 408, 412, 416, 420, 424, 432 };
uint8_t frequency_index = 0;

bool overclock() {
    hw_set_bits(&vreg_and_chip_reset_hw->vreg, VREG_AND_CHIP_RESET_VREG_VSEL_BITS);
    sleep_ms(10);
    return set_sys_clock_khz(frequencies[frequency_index] * 1000, true);
}

#define CLCK_PIN 29
#define D0 9
#define D1 8
#define D2 7
#define D3 6
#define D4 5
#define D5 4
#define D6 3
#define D7 2
#define WE 10

#include "title.h"

static void PWM_init_pin(uint pinN) {
    gpio_set_function(pinN, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(pinN);

    pwm_config c_pwm = pwm_get_default_config();
    pwm_config_set_clkdiv(&c_pwm, clock_get_hz(clk_sys) / (2.0 * 3'579'545));
    pwm_config_set_wrap(&c_pwm, 3);//MAX PWM value
    pwm_init(slice_num, &c_pwm, true);
    pwm_set_gpio_level(pinN, 2);

}

PIO pio = pio1;
uint sm = pio_claim_unused_sm(pio, true);

// The function to set up the PIO and load the program
void sn76489_init(uint pin_base) {
    const uint16_t out_instr = pio_encode_out(pio_pins, 8);
    const struct pio_program sn76489_program = {
            .instructions = &out_instr,
            .length = 1,
            .origin = -1,
    };

    const uint offset = pio_add_program(pio, &sn76489_program);
//    pio_gpio_init(pio, WE);

    for (int i = 0; i < 8; i++) {
        pio_gpio_init(pio, pin_base + i);

    }

    pio_sm_set_consecutive_pindirs(pio, sm, pin_base, 8, true);
//    pio_sm_set_consecutive_pindirs(pio, sm, WE, 1, true);

    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset + 0, offset + (sn76489_program.length - 1));

    sm_config_set_out_pins(&c, pin_base, 8);
//    sm_config_set_sideset_pins(&c, WE);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);
    sm_config_set_out_shift(&c, true, true, 8);

/*
    // Determine clock divider
    constexpr uint32_t max_pio_clk = 100 * MHZ;
    const uint32_t sys_clk_hz = clock_get_hz(clk_sys);
    const uint32_t clk_div = (sys_clk_hz + max_pio_clk - 1) / max_pio_clk;
    sm_config_set_clkdiv(&c, clk_div);
*/
    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);
}

static uint8_t  __aligned(4) reversed[] =
        {
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


//==============================================================
void send_sn76489(uint8_t byte) {
    gpio_put(WE, true);
    pio_sm_put(pio, sm, reversed[byte]);
    gpio_put(WE, false);
    busy_wait_us(23);
    gpio_put(WE, true);
}

#define HIGH 1
#define LOW 0

static inline void PutByte(uint8_t b) {
    gpio_put(D0, (b & 1) ? HIGH : LOW);
    gpio_put(D1, (b & 2) ? HIGH : LOW);
    gpio_put(D2, (b & 4) ? HIGH : LOW);
    gpio_put(D3, (b & 8) ? HIGH : LOW);
    gpio_put(D4, (b & 16) ? HIGH : LOW);
    gpio_put(D5, (b & 32) ? HIGH : LOW);
    gpio_put(D6, (b & 64) ? HIGH : LOW);
    gpio_put(D7, (b & 128) ? HIGH : LOW);
}

static inline void delay(unsigned int us) {

    busy_wait_ms(us);
}

//==============================================================
void SendByte(uint8_t b) {
    gpio_put(WE, HIGH);
    PutByte(b);
    gpio_put(WE, LOW);
    busy_wait_us(23);
    gpio_put(WE, HIGH);
}

//==============================================================
void SilenceAllChannels() {
    SendByte(0x9F);
    SendByte(0xBF);
    SendByte(0xDF);
    SendByte(0xFF);
}

//==============================================================
uint16_t ONESAMPLE = 23;
uint16_t Samples = 0;
uint16_t vgmpos = 0x40;

void loop() {
    uint8_t vgmdata = vgm_song[vgmpos];
    switch (vgmdata) {
        case 0x50: // 0x50 dd : PSG (SN76489/SN76496) write value dd
            vgmpos++;
            vgmdata = vgm_song[vgmpos];
            SendByte(vgmdata);
            vgmpos++;
            break;

        case 0x61: // 0x61 nn nn : Wait n samples, n can range from 0 to 65535
            vgmpos++;
            Samples = (uint16_t) (vgm_song[vgmpos] & 0x00FF);
            vgmpos++;
            Samples |= (uint16_t) ((vgm_song[vgmpos] << 8) & 0xFF00);
            vgmpos++;
            delay(Samples * 0.023);
            break;

        case 0x62: // wait 735 samples (60th of a second)
            vgmpos++;
            busy_wait_us(16666);
            break;

        case 0x63: // wait 882 samples (50th of a second)
            vgmpos++;
            busy_wait_us(20000);
            break;

        case 0x70: // 0x7n : wait n+1 samples, n can range from 0 to 15
            Samples = 1;
            vgmpos++;
            sleep_ms(Samples * ONESAMPLE);
            break;
        case 0x71:
            Samples = 2;
            vgmpos++;
            sleep_ms(Samples * ONESAMPLE);
            break;
        case 0x72:
            Samples = 3;
            vgmpos++;
            sleep_ms(Samples * ONESAMPLE);
            break;
        case 0x73:
            Samples = 4;
            vgmpos++;
            sleep_ms(Samples * ONESAMPLE);
            break;
        case 0x74:
            Samples = 5;
            vgmpos++;
            sleep_ms(Samples * ONESAMPLE);
            break;
        case 0x75:
            Samples = 6;
            vgmpos++;
            sleep_ms(Samples * ONESAMPLE);
            break;
        case 0x76:
            Samples = 7;
            vgmpos++;
            sleep_ms(Samples * ONESAMPLE);
            break;
        case 0x77:
            Samples = 8;
            vgmpos++;
            sleep_ms(Samples * ONESAMPLE);
            break;
        case 0x78:
            Samples = 9;
            vgmpos++;
            sleep_ms(Samples * ONESAMPLE);
            break;
        case 0x79:
            Samples = 10;
            vgmpos++;
            sleep_ms(Samples * ONESAMPLE);
            break;
        case 0x7a:
            Samples = 11;
            vgmpos++;
            sleep_ms(Samples * ONESAMPLE);
            break;
        case 0x7b:
            Samples = 12;
            vgmpos++;
            sleep_ms(Samples * ONESAMPLE);
            break;
        case 0x7c:
            Samples = 13;
            vgmpos++;
            sleep_ms(Samples * ONESAMPLE);
            break;
        case 0x7d:
            Samples = 14;
            vgmpos++;
            sleep_ms(Samples * ONESAMPLE);
            break;
        case 0x7e:
            Samples = 15;
            vgmpos++;
            sleep_ms(Samples * ONESAMPLE);
            break;
        case 0x7f:
            Samples = 16;
            vgmpos++;
            sleep_ms(Samples * ONESAMPLE);
            break;

        case 0x66: // 0x66 : end of sound data
            vgmpos = 0x40;
            SilenceAllChannels();
            delay(2000);
            break;

        default:
            break;
    } //end switch


}

int __time_critical_func(main)() {
    overclock();
    stdio_init_all();

    PWM_init_pin(CLCK_PIN);
    sn76489_init(D7);
/*

    gpio_init(D0);
    gpio_set_dir(D0, GPIO_OUT);

    gpio_init(D1);
    gpio_set_dir(D1, GPIO_OUT);

    gpio_init(D2);
    gpio_set_dir(D2, GPIO_OUT);

    gpio_init(D2);
    gpio_set_dir(D2, GPIO_OUT);

    gpio_init(D3);
    gpio_set_dir(D3, GPIO_OUT);

    gpio_init(D4);
    gpio_set_dir(D4, GPIO_OUT);

    gpio_init(D5);
    gpio_set_dir(D5, GPIO_OUT);

    gpio_init(D6);
    gpio_set_dir(D6, GPIO_OUT);

    gpio_init(D7);
    gpio_set_dir(D7, GPIO_OUT);
*/

    gpio_init(WE);
    gpio_set_dir(WE, GPIO_OUT);

    gpio_put(WE, true);
    SilenceAllChannels();
    while(1) {
//        if (data)
            send_sn76489(getchar());
    }
//    while (1) loop();
}

#include <cstdio>
#include <cstring>
#include <hardware/flash.h>
#include <hardware/structs/vreg_and_chip_reset.h>
#include <pico/multicore.h>
#include <pico/stdlib.h>
#include <hardware/pwm.h>
#include <hardware/structs/clocks.h>
#include <hardware/clocks.h>

uint16_t frequencies[] = { 272, 396, 404, 408, 412, 416, 420, 424, 432 };
uint8_t frequency_index = 0;

bool overclock() {
    hw_set_bits(&vreg_and_chip_reset_hw->vreg, VREG_AND_CHIP_RESET_VREG_VSEL_BITS);
    sleep_ms(10);
    return set_sys_clock_khz(frequencies[frequency_index] * 1000, true);
}

#define CLCK_PIN 0
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

#define HIGH 1
#define LOW 0

void PutByte(uint8_t b) {
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
    PWM_init_pin(CLCK_PIN);


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

    gpio_init(WE);
    gpio_set_dir(WE, GPIO_OUT);

    gpio_put(WE, true);
    SilenceAllChannels();
    while (1) loop();
}

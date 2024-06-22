#include "74hc595.h"
#include "hardware/clocks.h"
#include "pico/platform.h"

static const uint16_t program_instructions595[] = {
        //     .wrap_target
        0x80a0, //  0: pull   block           side 0
        0x6001, //  1: out    pins, 1         side 0
        0x1041, //  2: jmp    x--, 1          side 2
        0xe82f, //  3: set    x, 15           side 1
        //     .wrap
};

static const struct pio_program program595 = {
        .instructions = program_instructions595,
        .length = 4,
        .origin = -1,
};

void init_74hc595() {
    uint offset = pio_add_program(PIO_74HC595, &program595);
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset, offset + (program595.length - 1));
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);

    // sm_config_set_in_shift(&c, true, false, 32);//??????  
    // sm_config_set_in_pins(&c, PIN_PS2_DATA);

//     pio_gpio_init(pioAY595, CLK_LATCH_595_BASE_PIN);
//     pio_gpio_init(pioAY595, CLK_LATCH_595_BASE_PIN+1);
// //pio_sm_set_consecutive_pindirs(pio, sm, pin, 1, true);
//     sm_config_set_sideset_pins(&c, CLK_LATCH_595_BASE_PIN);

//     sm_config_set_set_pins(&c, CLK_LATCH_595_BASE_PIN, 2);
//     sm_config_set_sideset(&c, 2, false, false);


    //настройка side set
    sm_config_set_sideset_pins(&c, CLK_LATCH_595_BASE_PIN);
    sm_config_set_sideset(&c, 2, false, false);

    for (int i = 0; i < 2; i++) {
        pio_gpio_init(PIO_74HC595, CLK_LATCH_595_BASE_PIN + i);
    }

    pio_sm_set_pins_with_mask(PIO_74HC595, SM_74HC595, 3u << CLK_LATCH_595_BASE_PIN, 3u << CLK_LATCH_595_BASE_PIN);
    pio_sm_set_pindirs_with_mask(PIO_74HC595, SM_74HC595, 3u << CLK_LATCH_595_BASE_PIN, 3u << CLK_LATCH_595_BASE_PIN);
    //

    pio_gpio_init(PIO_74HC595, DATA_595_PIN);//резервируем под выход PIO

    pio_sm_set_consecutive_pindirs(PIO_74HC595, SM_74HC595, DATA_595_PIN, 1, true);//конфигурация пинов на выход

    sm_config_set_out_shift(&c, false, false, 32);
    sm_config_set_out_pins(&c, DATA_595_PIN, 1);

    pio_sm_init(PIO_74HC595, SM_74HC595, offset, &c);
    pio_sm_set_enabled(PIO_74HC595, SM_74HC595, true);

    pio_sm_set_clkdiv(PIO_74HC595, SM_74HC595, clock_get_hz(clk_sys) / SHIFT_SPEED);
    PIO_74HC595->txf[SM_74HC595] = 0;


};

void __not_in_flash_func(write_74hc595)(uint16_t data) {
    PIO_74HC595->txf[SM_74HC595] = data << 16;
}
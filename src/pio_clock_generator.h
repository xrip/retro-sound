#pragma once

#if !PICO_NO_HARDWARE
#include <hardware/clocks.h>
#include "hardware/pio.h"
#endif

#define PIO_BASE_CLOCK pio0
#define SM_BASE_CLOCK 1

#define BASE_CLOCK (14'318)

#define BASE_CLOCK_PIN (27)


#define base_clock_wrap_target 0
#define base_clock_wrap 7

static const uint16_t base_clock_program_instructions[] = {
    //     .wrap_target
    0xa042, //  0: nop                    side 0     
    0xa442, //  1: nop                    side 1     
    0xa842, //  2: nop                    side 2     
    0xac42, //  3: nop                    side 3     
    0xb042, //  4: nop                    side 4     
    0xb442, //  5: nop                    side 5     
    0xb842, //  6: nop                    side 6     
    0x1c00, //  7: jmp    0               side 7     
    //     .wrap
};

#if !PICO_NO_HARDWARE
static constexpr struct pio_program base_clock_program = {
    .instructions = base_clock_program_instructions,
    .length = 8,
    .origin = -1,
};

static inline pio_sm_config base_clock_program_get_default_config(const uint offset) {
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset + base_clock_wrap_target, offset + base_clock_wrap);
    //настройка side set

    sm_config_set_sideset(&c, 3, false, false);
    return c;
}

static inline void base_clock_program_init(const PIO pio, const uint sm, const uint offset, const uint pin) {
    pio_sm_config c = base_clock_program_get_default_config(offset);
    // Map the state machine's OUT pin group to one pin, namely the `pin`
    // parameter to this function.
    sm_config_set_out_pins(&c, pin, 3);
    // Set this pin's GPIO function (connect PIO to the pad)
    for (int i = 0; i < 3; i++) {
        pio_gpio_init(pio, pin + i);
    }
    sm_config_set_sideset_pins(&c, pin);
    // Set the pin direction to output at the PIO
    pio_sm_set_consecutive_pindirs(pio, sm, pin, 3, true);

    // Load our configuration, and jump to the start of the program
    pio_sm_init(pio, sm, offset, &c);

    pio_sm_set_clkdiv(pio, sm, clock_get_hz(clk_sys) / (2 * BASE_CLOCK * KHZ));

    // Set the state machine running
    pio_sm_set_enabled(pio, sm, false);
    pio_sm_set_clkdiv(pio, sm, clock_get_hz(clk_sys) / (2 * BASE_CLOCK * KHZ));
    pio_sm_set_enabled(pio, sm, true);
}

#endif

static inline void ini_chips_clocks() {
    const uint offset = pio_add_program(PIO_BASE_CLOCK, &base_clock_program);
    base_clock_program_init(PIO_BASE_CLOCK,SM_BASE_CLOCK, offset,BASE_CLOCK_PIN);
}

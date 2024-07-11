#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include "hardware/pio.h"

#define PIO_74HC595 pio0
#define SM_74HC595 1

// 10Mhz
#define SHIFT_SPEED (10*1500000)

#define CLK_LATCH_595_BASE_PIN (26)
#define DATA_595_PIN (28)

void init_74hc595();

void write_74hc595(uint16_t data, uint16_t delay_us);

#ifdef __cplusplus
}
#endif
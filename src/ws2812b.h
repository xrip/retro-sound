#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "hardware/dma.h"

#define USE_WS2812C_PIO

// #define WS2812_PIN 23     // rp2040 black
#define WS2812_PIN 16       // rp2040 zero

#define LED_COUNT 1

// uint32_t colours[LED_COUNT] = {0};

uint32_t led_buffer[LED_COUNT] = {0};

#define ws2812c_wrap_target 0
#define ws2812c_wrap 4

static const uint16_t ws2812c_program_instructions[] = {
            //     .wrap_target
    0x80a0, //  0: pull   block                      
    0x6068, //  1: out    null, 8                    
    0xa00b, //  2: mov    pins, !null                
    0x6001, //  3: out    pins, 1                    
    0x10e2, //  4: jmp    !osre, 2        side 0     
            //     .wrap
};

static const struct pio_program ws2812c_program = {
    .instructions = ws2812c_program_instructions,
    .length = 5,
    .origin = -1,
};

static inline pio_sm_config ws2812c_program_get_default_config(uint offset) {
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset + ws2812c_wrap_target, offset + ws2812c_wrap);
    sm_config_set_sideset(&c, 2, true, false);
    return c;
}

void ws2812c_program_init(PIO pio,uint sm, uint offset, uint pin)
{
    pio_sm_config cfg =ws2812c_program_get_default_config(offset);
    sm_config_set_out_shift(&cfg, false, false, 32);
    sm_config_set_clkdiv(&cfg, (clock_get_hz(clk_sys) / 1e6f) * 1.25f / 3.0f);  //1.25us / 3
    sm_config_set_out_pins(&cfg, pin, 1);
    sm_config_set_sideset_pins(&cfg, pin);
    pio_gpio_init(pio, pin);
    pio_sm_set_consecutive_pindirs(pio, sm, pin, 1, true);
    pio_sm_init(pio, sm, offset, &cfg);
}    

//-----------------------------------------------------------------------------------------------
static inline void ws2812_hsv2rgb(uint8_t hue, uint8_t sat, uint8_t bri, uint8_t *_r, uint8_t *_g, uint8_t *_b)
{
	if(sat==0)
	{
		*_r = *_g = *_b = bri;
		return;
	}
	float h = (float)hue/255;
	float s = (float)sat/255;
	float b = (float)bri/255;
	
	int i = (int)(h*6);
	float f = h*6 - (float)i;
	uint8_t p = (uint8_t)(b * (1 - s) * 255.0);
	uint8_t q = (uint8_t)(b * (1 - f * s) * 255.0);
	uint8_t t = (uint8_t)(b * (1-(1-f) * s) * 255.0);
	
	switch (i%6)
	{
		case 0: *_r = bri, *_g = t, *_b = p; break;
		case 1: *_r = q, *_g = bri, *_b = p; break;
		case 2: *_r = p, *_g = bri, *_b = t; break;
		case 3: *_r = p, *_g = q, *_b = bri; break;
		case 4: *_r = t, *_g = p, *_b = bri; break;
		default: *_r = bri, *_g = p, *_b = q; break;
	}
 }
 //---заполнение отдельного пикселя в буфере цветом----------
 static inline void 	ws2812_pixel_rgb_to_buf_dma(uint8_t Rpixel , uint8_t Gpixel, uint8_t Bpixel, uint8_t PosX)
 {

    if (PosX < LED_COUNT)    {led_buffer[PosX] = (Gpixel<<16)|(Rpixel<<8)|(Bpixel);}

 }

PIO pio = pio0;
const uint sm = 0;
const int led_dma_chan = 0;
dma_channel_config dma_ch0 = dma_channel_get_default_config(led_dma_chan);

//-------------------------------------------------------------------------------
//  i = номер светодиода?
//  hue = цвет 0-30-60-90-120-150-180-210-240
//  bri  = яркость
//  sat = насыщенность
static inline void set_ws2812b_HSV(uint8_t i, uint8_t _hue, uint8_t sat, uint8_t _bri)
{
	uint8_t _r, _g, _b;																//init buffer color
	ws2812_hsv2rgb(_hue, sat, _bri, &_r, &_g, &_b);			//get RGB color
	ws2812_pixel_rgb_to_buf_dma(_r,_g,_b, i);					//set color
    // старт 
    dma_channel_configure (led_dma_chan, &dma_ch0, &pio->txf[sm], led_buffer, LED_COUNT, true);
}

//-------------------------------------------------------------------------------
static inline void init_ws2812b()
{
    uint offset = pio_add_program(pio, &ws2812c_program);
    ws2812c_program_init(pio, sm, offset, WS2812_PIN);

    // initialise the used GPIO pin to LOW
    gpio_put(WS2812_PIN, false);

    //configure DMA to copy the LED buffer to the PIO state machine's FIFO

    channel_config_set_transfer_data_size(&dma_ch0, DMA_SIZE_32);
    channel_config_set_read_increment(&dma_ch0, true);
    channel_config_set_write_increment(&dma_ch0, false);
    channel_config_set_dreq(&dma_ch0, DREQ_PIO0_TX0);

    //run the state machine
    pio_sm_set_enabled(pio, sm, true);

}

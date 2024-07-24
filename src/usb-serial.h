#ifndef USB_UART_H
#define USB_UART_H

#include <hardware/irq.h>
#include <hardware/structs/sio.h>
#include <hardware/uart.h>
#include <pico/multicore.h>
#include <pico/stdlib.h>
#include <string.h>
#include <tusb.h>
#include <fifo.h>

#define LED_PIN 25
#define LED1_PIN 24

#define BUFFER_SIZE 1024

fifo_typedef(uint8_t, fifo);

typedef struct {
	uart_inst_t *const inst;
	uint8_t tx_pin;
	uint8_t rx_pin;
} uart_id_t;

typedef struct {
	uint8_t uart_buffer[BUFFER_SIZE];
	mutex_t uart_mtx;
	fifo uart_fifo;
	fifo* uart_fifo_ptr;
	uint8_t usb_buffer[BUFFER_SIZE];
	mutex_t usb_mtx;
	fifo usb_fifo;
	fifo* usb_fifo_ptr;
} uart_data_t;

void usb_serial_init();
void usb_serial_handle();
uint8_t usb_com_read();
void usb_com_write_char(uint8_t val);
void usb_com_write(uint8_t* buf, uint32_t len);
void usb_com_print(char* buf);
uint32_t usb_com_usb_buff_index();
uint32_t usb_com_uart_buff_index();

#endif
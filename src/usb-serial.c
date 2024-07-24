// SPDX-License-Identifier: MIT
/*
 * Copyright 2021 Álvaro Fernández Rojas <noltari@gmail.com>
 */

#include <usb-serial.h>

const uart_id_t UART_ID[CFG_TUD_CDC] = {
	{
		.inst = uart0,
		.tx_pin = 0,
		.rx_pin = 1,
	}, {
		.inst = uart1,
		.tx_pin = 4,
		.rx_pin = 5,
	}, {  // fake uart for communication with the host
		.inst = 0,
	}
};

uart_data_t UART_DATA[CFG_TUD_CDC];

void usb_read_bytes(uint8_t itf) {
	uint32_t len = tud_cdc_n_available(itf);

	if (len) {

		uart_data_t *ud = &UART_DATA[itf];
		uint8_t val;
		mutex_enter_blocking(&ud->usb_mtx);
		while (len-- && !fifo_is_full(ud->usb_fifo_ptr)) {
			val = tud_cdc_n_read_char(itf);
			fifo_write(ud->usb_fifo_ptr, val);
		}
		mutex_exit(&ud->usb_mtx);
 	}
}

void usb_write_bytes(uint8_t itf) {
	uart_data_t *ud = &UART_DATA[itf];

	if (!fifo_is_empty(ud->uart_fifo_ptr)) {
		uint8_t val;
		mutex_enter_blocking(&ud->uart_mtx);
		while (!fifo_is_empty(ud->uart_fifo_ptr)) {
			fifo_read(ud->uart_fifo_ptr, val);
			tud_cdc_n_write_char(itf, val);
		}
		mutex_exit(&ud->uart_mtx);
		tud_cdc_n_write_flush(itf);
	}
}

void uart_read_bytes(uint8_t itf) {
	const uart_id_t *ui = &UART_ID[itf];

	if (ui->inst != 0 && uart_is_readable(ui->inst)) {
		uart_data_t *ud = &UART_DATA[itf];
		mutex_enter_blocking(&ud->uart_mtx);
		while (uart_is_readable(ui->inst) && !fifo_is_full(ud->uart_fifo_ptr)) {
			fifo_write(ud->uart_fifo_ptr, uart_getc(ui->inst));
		}
		mutex_exit(&ud->uart_mtx);
	}
}

void uart_write_bytes(uint8_t itf) {
	uart_data_t *ud = &UART_DATA[itf];
	const uart_id_t *ui = &UART_ID[itf];

	if (ui->inst != 0 && !fifo_is_empty(ud->usb_fifo_ptr)) {
		uint8_t val;
		mutex_enter_blocking(&ud->usb_mtx);

		// disable uart rx to not loop back the transmitted data
		if (ui->inst == uart1) {
			gpio_set_oeover(ui->rx_pin, GPIO_OVERRIDE_HIGH);
		}

		for (int i = 0; i < fifo_count(ud->usb_fifo_ptr); i++) {
			fifo_read(ud->usb_fifo_ptr, val);
			uart_putc(ui->inst, val);
		}
		uart_tx_wait_blocking(ui->inst);

		// re enable uart rx
		if (ui->inst == uart1) {
			gpio_set_oeover(ui->rx_pin, GPIO_OVERRIDE_NORMAL);
		}

		mutex_exit(&ud->usb_mtx);
	}
}

// read byte from usb
uint8_t usb_com_read(){
	if (tud_cdc_n_connected(0)){
		uart_data_t *ud = &UART_DATA[2];
		uint8_t val;
		mutex_enter_blocking(&ud->usb_mtx);
		fifo_read(ud->usb_fifo_ptr, val);
		mutex_exit(&ud->usb_mtx);
		return val;
	}
//	return -1;
}

void usb_com_write(uint8_t* buf, uint32_t len){
	if (tud_cdc_n_connected(2)){
		uart_data_t *ud = &UART_DATA[2];
		int i = 0;

		mutex_enter_blocking(&ud->uart_mtx);
		while (i < len && !fifo_is_full(ud->uart_fifo_ptr)) {
			fifo_write(ud->uart_fifo_ptr, buf[i]);
			i++;
		}
		mutex_exit(&ud->uart_mtx);
	}
}

void usb_com_write_char(uint8_t val){
	if (tud_cdc_n_connected(2)){
		uart_data_t *ud = &UART_DATA[2];

		mutex_enter_blocking(&ud->uart_mtx);
		fifo_write(ud->uart_fifo_ptr, val);
		mutex_exit(&ud->uart_mtx);
	}
}

void usb_com_print(char* buf){
	if (tud_cdc_n_connected(2)){
		uart_data_t *ud = &UART_DATA[2];
		int i = 0;

		mutex_enter_blocking(&ud->uart_mtx);
		while (buf[i] != '\0' && !fifo_is_full(ud->uart_fifo_ptr)) {
			fifo_write(ud->uart_fifo_ptr, buf[i]);
			i++;
		}
		mutex_exit(&ud->uart_mtx);
	}
}

uint32_t usb_com_usb_buff_index(){
	uart_data_t *ud = &UART_DATA[2];
	mutex_enter_blocking(&ud->usb_mtx);
	uint32_t count = fifo_count(ud->usb_fifo_ptr);
	mutex_exit(&ud->usb_mtx);
	return count;
}

uint32_t usb_com_uart_buff_index(){
	uart_data_t *ud = &UART_DATA[2];
	mutex_enter_blocking(&ud->uart_mtx);
	uint32_t count = fifo_count(ud->uart_fifo_ptr);
	mutex_exit(&ud->uart_mtx);
	return count;
}

void init_uart_data(uint8_t itf) {
	const uart_id_t *ui = &UART_ID[itf];
	uart_data_t *ud = &UART_DATA[itf];

	if (ui->inst != 0) {
		/* Pinmux */
		gpio_set_function(ui->tx_pin, GPIO_FUNC_UART);
		gpio_set_function(ui->rx_pin, GPIO_FUNC_UART);
	}

	/* Initialize FIFOs */
	ud->uart_fifo_ptr = &ud->uart_fifo;
	fifo_init(ud->uart_fifo_ptr, BUFFER_SIZE, uint8_t, ud->uart_buffer);
	ud->usb_fifo_ptr = &ud->usb_fifo;
	fifo_init(ud->usb_fifo_ptr, BUFFER_SIZE, uint8_t, ud->usb_buffer);

	/* Mutex */
	mutex_init(&ud->uart_mtx);
	mutex_init(&ud->usb_mtx);

	if (ui->inst != 0) {
		/* UART start */
		uart_init(ui->inst, 115200);
		uart_set_hw_flow(ui->inst, false, false);
		uart_set_format(ui->inst, 8 ,1, 0);
	}
}


void core1_entry(void) {
	tusb_init();

	while (1) {
		tud_task();


		for (int itf = 0; itf < CFG_TUD_CDC; itf++) {
			if (tud_cdc_n_connected(itf)) {
				usb_read_bytes(itf);
//				usb_write_bytes(itf);
//				uart_read_bytes(itf);
//				uart_write_bytes(itf);
			}
		}
		//gpio_put(LED_PIN, 0);
	}
}

/*
void usb_serial_handle(){
	tud_task();

	gpio_put(LED_PIN, 1);
	for (int itf = 0; itf < CFG_TUD_CDC; itf++) {
		if (tud_cdc_n_connected(itf)) {
			usb_read_bytes(itf);
			usb_write_bytes(itf);
			uart_read_bytes(itf);
			uart_write_bytes(itf);
		}
	}
	gpio_put(LED_PIN, 0);
}
*/

void usb_serial_init() {
	for (int itf = 0; itf < CFG_TUD_CDC; itf++){
		init_uart_data(itf);
	}
	
	gpio_init(LED_PIN);
	gpio_set_dir(LED_PIN, GPIO_OUT);
	gpio_init(LED1_PIN);
	gpio_set_dir(LED1_PIN, GPIO_OUT);

//	tusb_init();
//
//	multicore_launch_core1(core1_entry);
}
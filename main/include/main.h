//
// Created by asus on ۰۹/۰۵/۲۰۲۱.
//

#ifndef HELLO_WORLD_MAIN_H
#define HELLO_WORLD_MAIN_H

#include "app_address.h"
#include "ir_rx.h"
#define TAG "HELLO"

#define WAIT_FOR_NETWORK() while(!WIFI_IS_READY()) { vTaskDelay(5000 / portTICK_PERIOD_MS); }
#define PIN_YELLOW      GPIO_NUM_2
#define PIN_GREEN       GPIO_NUM_4
#define PIN_RELAY       GPIO_NUM_5

#define PIN_IR_LFT     GPIO_NUM_12
#define PIN_IR_RHT     GPIO_NUM_13
#define PIN_IR_INP     GPIO_NUM_14

#define PIN_BKL        GPIO_NUM_16

#define RELAY_SENSOR    1
#define BCKLT_SENSOR    2
#define CTRL_SENSOR     0


#define SELECT_NONE_TX()   gpio_set_level(PIN_IR_LFT,0);gpio_set_level(PIN_IR_RHT, 0);
#define SELECT_LEFT_TX()   gpio_set_level(PIN_IR_LFT,1);gpio_set_level(PIN_IR_RHT, 0);
#define SELECT_RIGHT_TX()  gpio_set_level(PIN_IR_LFT,0);gpio_set_level(PIN_IR_RHT, 0);
#define GPIO_OUTPUT_IO_12    12
#define GPIO_OUTPUT_IO_13    13
#define GPIO_OUTPUT_PIN_SEL  ((1ULL << GPIO_OUTPUT_IO_12) | (1ULL << GPIO_OUTPUT_IO_13))



struct command {
	command(unsigned s, unsigned v) : sensor(s), value(v) { order = 0; }
	inline command(int data) { this->data = data; }
	union {
		uint8_t data = 0;
		struct {
			unsigned sensor: 2; // 0 relay, 1 backlight, 2 reconfig
			unsigned value: 3; // 0:%0(off) 1:%100(on) 2:%20 .3%30 4:%40 5:%50 6:%70. 7:%70
			unsigned order: 3; // 0-7
		};
	};
	app_address to;
};

void append_to_post_queue(command *post);

bool wait_a_while_for_wifi(unsigned int millis);

char *serialize(command &c);

[[noreturn]] void start_network_task();
[[noreturn]] void start_switching_task();
bool ExecuteDelayedTask(void (*func)(), uint8_t seconds, uint8_t stkBlocks, const char *name);

#ifdef __cplusplus
extern "C" {
#endif
 void app_main(void);

#ifdef __cplusplus
}
#endif
#endif //HELLO_WORLD_MAIN_H

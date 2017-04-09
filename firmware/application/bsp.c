#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include "bsp.h"


void bsp_init(void) {
    // init clock
    // todo

    rcc_periph_clock_enable(RCC_GPIOB);
    rcc_periph_clock_enable(RCC_GPIOB);

    // User button pin as input, pull-down done by external hardware
    gpio_set_mode(USER_BT_PORT, GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT, USER_BT_PIN);
    // User LED pin as output
    gpio_set_mode(USER_LED_PORT, GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, USER_LED_PIN);
}


void bsp_set_led_state(bool state) {
    (true == state) ? gpio_set(USER_LED_PORT, USER_LED_PIN) : gpio_clear(USER_LED_PORT, USER_LED_PIN);
}



void bsp_led_toggle(void) {
    gpio_toggle(USER_LED_PORT, USER_LED_PIN);
}



bool bsp_get_bt_state(void) {
    uint16_t state = gpio_get(USER_BT_PORT, USER_BT_PIN);
    // todo software handling of debouncing is required while there is some hardware to handle this?
    return (0 != state) ? true : false;
}

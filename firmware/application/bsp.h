#ifndef BSP_H_
#define BSP_H_

#include <stdbool.h>

#define USER_BT_PORT GPIOB
#define USER_BT_PIN GPIO5

#define USER_LED_PORT GPIOB
#define USER_LED_PIN GPIO6

/// Initializes board specific hardware, like user button, LED.
void bsp_init(void);

/**
 * Controls state of user LED.
 * @param state if it's true then user LED will be turned on, otherwise LED will be turned off.
 */
void bsp_set_led_state(bool state);

/**
 * Toggles state of user LED.
 */
void bsp_led_toggle(void);

/**
 * Returns state of user button.
 * @return true if button is pressed, false otherwise.
 */
bool bsp_get_bt_state(void);

#endif //BSP_H_

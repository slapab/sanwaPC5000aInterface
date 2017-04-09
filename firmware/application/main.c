/*
 * This file is part of the libopencm3 project.
 *
 * Copyright (C) 2010 Gareth McMullin <gareth@blacksphere.co.nz>
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <string.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/usb/usbd.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/systick.h>

#include "usb_cdc_dev.h"
#include "systick_local.h"
#include "bsp.h"
#include "ir_interface.h"
#include "bm_dmm_protocol.h"


int main(void) {
    usbd_device* usbd_dev = NULL;
    rcc_clock_setup_in_hsi_out_48mhz();

    // Configures and start SysTick
    st_init(1000, 48000000);

    rcc_periph_clock_enable(RCC_GPIOA);
    rcc_periph_clock_enable(RCC_AFIO);

    AFIO_MAPR |= AFIO_MAPR_SWJ_CFG_JTAG_OFF_SW_ON;

    // init board specific hardware
    bsp_init();

    // init ir interface
    ir_itf_init();
    uint8_t ir_raw_data_buff[16] = {0};
    data_resp_pkt bm_data = {0};

    // LED on
    bsp_set_led_state(true);

    usbd_dev = usb_cdc_init();

    systick_t startPoint = st_get_ticks();

    // allow USB to submit on host OS
    while ((systick_t)(st_get_ticks() - startPoint) <= 2000) {
        usbd_poll(usbd_dev);
    }

    startPoint = st_get_ticks();
    while (1) {
        usbd_poll(usbd_dev);

//        if ((systick_t)(st_get_ticks() - startPoint) >= 100) {
//            bsp_led_toggle();
//
//            startPoint = st_get_ticks();
//        }

        // testing ir_interface
        if (true == ir_itf_read_blocking(ir_raw_data_buff, sizeof(ir_raw_data_buff)/sizeof(ir_raw_data_buff[0]))) {
            if (BM_PKG_CREATED == bm_create_pkt(ir_raw_data_buff, sizeof(ir_raw_data_buff)/sizeof(ir_raw_data_buff[0]), &bm_data)) {
                if ((systick_t)(st_get_ticks() - startPoint) >= 100) {
                                    bsp_led_toggle();

                                    startPoint = st_get_ticks();
                }
            }
        }

    }

    return 0;
}

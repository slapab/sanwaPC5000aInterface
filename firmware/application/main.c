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
#include <assert.h>
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
#include "check_data_req.h"

#define FAKE_RESPONSE 0
#if 1 == FAKE_RESPONSE
// AC 13.722V
static const data_resp_pkt example_voltageReading1 = {
        .header = {.dle = 0x10, .stx = 0x02, .cmd = 0x00, .dataLen = BM_NORMAL_PACKET_DATA_LENGTH},
        .func[0] = 0x05 /* AC V */, 0x00, 0x00, 0x00,
        .valSign = 0x20, /*no sign*/
        .asciiAndTailLong = {
                .d1 = 0x31, 0x2E, 0x33, 0x37, 0x37, 0x32, 0x20, 0x45, 0x2B, 0x31,
                .pktTail = {.chkSum = 0x44, .dle = 0x10, .etx = 0x03}
        }
};
#endif


/// Number of data requests received from host
static int dataRequestNo = 0;

/// Callback function called when received some data by USB-CDC protocol
static void cdcacm_rx_callback(usbd_device *usbd_dev, uint8_t ep) {
    (void)ep;

    static uint8_t buff[CDC_DATA_BUFFER_LEN] = {0};

    // read received bytes from usb buffer
    int len = usbd_ep_read_packet(usbd_dev, CDC_DATA_IN_EP, buff, CDC_DATA_BUFFER_LEN);

    // check received bytes for 'dmm-data' request
    if (len > 0) {
        dataRequestNo += check_buffer_for_data_request(buff, len);
    }
}



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
    ir_itf_init_nb();

    uint8_t ir_raw_data_buff[IR_DATA_BYTES] = {0};
    data_resp_pkt bm_data = {0};

    // LED on
    bsp_set_led_state(true);

    // register custom callback for USB-DATA-RECEIVED event
    usb_cdc_register_data_in_callback(cdcacm_rx_callback);
    // init USB device
    usbd_dev = usb_cdc_init();
    assert(NULL != usbd_dev);



    systick_t startPoint = st_get_ticks();
    // allow USB to submit on host OS
    while ((systick_t)(st_get_ticks() - startPoint) <= 2000) {
        usbd_poll(usbd_dev);
    }

    startPoint = st_get_ticks();
    while (1) {
        usbd_poll(usbd_dev);

        switch(ir_itf_get_status()) {
        case IR_ITF_READY:
            if (dataRequestNo > 0) {
                --dataRequestNo;

    #if 1 == FAKE_RESPONSE

                // simulate data acquisition
                if ((systick_t)(st_get_ticks() - startPoint) >= 350) {

                    usbd_ep_write_packet(usbd_dev, CDC_DATA_OUT_EP, (void*)&example_voltageReading1, sizeof(data_resp_pkt));
                    startPoint = st_get_ticks();
                }
    #else
                memset((void*)ir_raw_data_buff, 0, IR_DATA_BYTES);
                ir_itf_start_read_nb(ir_raw_data_buff, IR_DATA_BYTES);
    #endif

            }
        break;
        case IR_ITF_DONE: {
#if INTERFACE_VER1 == USING_INTERFACE_VER
            // Inverse bits in raw data -> DMM transmits '0' when turns its IR LED on. So with this version of hardware
            // read bit of value '1' is in fact bit of value '0'.
            for (int i = 0; i < IR_DATA_BYTES; ++i) {
                ir_raw_data_buff[i] = ~ir_raw_data_buff[i];
            }
#endif
            // convert raw data to the brymen packet
            if (BM_PKG_CREATED == bm_create_pkt(ir_raw_data_buff, IR_DATA_BYTES, &bm_data)) {
                usbd_ep_write_packet(usbd_dev, CDC_DATA_OUT_EP, (void*)&bm_data, sizeof(bm_data));
            }
        } // end of case IR_ITF_DONE
        break;
        default: break;
        } // end of switch


    }

    return 0;
}

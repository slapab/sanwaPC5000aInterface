#ifndef USB_CDC_DEV_H_
#define USB_CDC_DEV_H_

#include <libopencm3/usb/usbd.h>
#include <stdbool.h>

#define CDC_DATA_OUT_EP 0x82
#define CDC_DATA_IN_EP 0x01
#define CDC_COMM_EP 0x83

usbd_device* usb_cdc_init(void);

bool usb_cdc_register_data_in_callback(usbd_device* usbd_dev, usbd_endpoint_callback callback);

#endif //USB_CDC_DEV_H_

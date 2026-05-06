/**
 * @file usb_hid_mouse.h
 * @brief USB FS HID mouse test helper (for RL_SPORT test mode)
 */
#ifndef _USB_HID_MOUSE_H_
#define _USB_HID_MOUSE_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* Public API for test mode */
    void UsbHidMouse_TestStart(void);
    void UsbHidMouse_TestStop(void);
    void UsbHidMouse_TestUpdate(void);
    uint8_t UsbHidMouse_TestIsActive(void);

#ifdef __cplusplus
}
#endif

#endif /* _USB_HID_MOUSE_H_ */

#pragma once
#include <Adafruit_TinyUSB.h>

namespace jiggy_stealth {
// Build the complete descriptor set before the native USB controller starts.
void configureUsbDevice(Adafruit_USBD_HID& hid);
void connectUsbDevice();
void disconnectUsbDevice();
}

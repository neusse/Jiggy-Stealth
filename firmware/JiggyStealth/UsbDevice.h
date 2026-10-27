#pragma once
#include <Adafruit_TinyUSB.h>
#include "BridgeLogic.h"

namespace jiggy_stealth {
// Build the complete descriptor set before the native USB controller starts.
// Reconfiguration is permitted only while disconnected. Rebuilds both the
// HID report pointer AND the configuration's HID report-length field.
bool configureUsbDevice(Adafruit_USBD_HID& hid, Mode mode = Mode::Movement);
Mode usbHidMode();
bool keyboardHidEnabled();
bool usbReportAllowed(uint8_t reportId);
void connectUsbDevice();
void disconnectUsbDevice();
}

#include "UsbDevice.h"
#include "Config.h"

#ifndef USB_STARTUP_WRAPPING
#error Build with scripts/build.ps1 so automatic CDC startup is suppressed.
#endif

extern "C" {
void __real_TinyUSB_Device_Task(void);

// Arduino-Pico normally starts TinyUSB with CDC before setup(). Defer the
// controller itself so production never briefly enumerates a serial port.
void __wrap_TinyUSB_Device_Init(uint8_t) {}

void __wrap_TinyUSB_Device_Task(void) {
  // Arduino yield() may run while the donor is still being discovered. The
  // RP2040 port's USB mutex does not exist until controller initialization.
  if (TinyUSBDevice.isInitialized()) __real_TinyUSB_Device_Task();
}
}

namespace jiggy_stealth {
void configureUsbDevice(Adafruit_USBD_HID& hid) {
  TinyUSBDevice.clearConfiguration();
  // Keep HID at MI_00 in both profiles. Diagnostic CDC follows at MI_01/02.
  hid.begin();
  TinyUSBDevice.setProductDescriptor(config::ProjectName);
#if JIGGY_DIAGNOSTICS
  Serial.begin(115200);
#endif
  TinyUSBDevice.setConfigurationMaxPower(250);
}

void connectUsbDevice() {
  if (!TinyUSBDevice.isInitialized()) {
    // Do not call TinyUSBDevice.begin(): that would reset our descriptors and
    // add CDC again. Identity is already selected before this first connect.
    TinyUSB_Port_InitDevice(0);
  }
  TinyUSBDevice.attach();
}

void disconnectUsbDevice() {
  if (TinyUSBDevice.isInitialized()) TinyUSBDevice.detach();
}
}

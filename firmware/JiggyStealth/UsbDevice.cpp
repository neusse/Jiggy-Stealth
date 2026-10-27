#include "UsbDevice.h"
#include "Config.h"
#include "HidDescriptors.h"

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
namespace {
bool hidRegistered = false, attached = false;
Mode descriptorMode = Mode::Movement;
}

Mode usbHidMode() { return descriptorMode; }
bool keyboardHidEnabled() { return descriptorMode == Mode::Shift; }
bool usbReportAllowed(uint8_t reportId) {
  return reportId == config::MouseReportId ||
         (reportId == config::KeyboardReportId && keyboardHidEnabled());
}

bool configureUsbDevice(Adafruit_USBD_HID& hid, Mode mode) {
  if (attached) return false;
#if JIGGY_DIAGNOSTICS
  // end() resets both the configuration and the CDC instance registry.
  Serial.end();
#else
  TinyUSBDevice.clearConfiguration();
#endif
  if (mode == Mode::Shift) hid.setReportDescriptor(HidDescriptor.data(), static_cast<uint16_t>(HidDescriptor.size()));
  else hid.setReportDescriptor(MouseHidDescriptor, sizeof(MouseHidDescriptor));
  // Keep HID at MI_00 in both profiles. Diagnostic CDC follows at MI_01/02.
  // begin() is a no-op after its first call, so rebuild an already registered
  // HID interface explicitly to update wDescriptorLength on each mode change.
  if (hidRegistered) {
    if (!TinyUSBDevice.addInterface(hid)) return false;
  } else {
    if (!hid.begin()) return false;
    hidRegistered = true;
  }
  TinyUSBDevice.setProductDescriptor(config::ProjectName);
#if JIGGY_DIAGNOSTICS
  Serial.begin(115200);
#endif
  TinyUSBDevice.setConfigurationMaxPower(250);
  descriptorMode = mode;
  return true;
}

void connectUsbDevice() {
  if (!TinyUSBDevice.isInitialized()) {
    // Do not call TinyUSBDevice.begin(): that would reset our descriptors and
    // add CDC again. Identity is already selected before this first connect.
    TinyUSB_Port_InitDevice(0);
  }
  TinyUSBDevice.attach();
  attached = true;
}

void disconnectUsbDevice() {
  if (TinyUSBDevice.isInitialized()) TinyUSBDevice.detach();
  attached = false;
}
}

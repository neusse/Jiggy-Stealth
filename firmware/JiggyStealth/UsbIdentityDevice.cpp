#include <Adafruit_TinyUSB.h>
#include <hardware/sync.h>
#include "UsbIdentity.h"
#include "Config.h"

#ifndef USB_IDENTITY_WRAPPING
#error Build with scripts/build.ps1 so the USB identity descriptor wrappers are linked.
#endif

namespace {
jiggy_stealth::UsbIdentity currentIdentity;
}

namespace jiggy_stealth {
void setVisibleIdentity(const UsbIdentity& identity) {
  uint32_t interruptState = save_and_disable_interrupts();
  currentIdentity = identity;
  restore_interrupts(interruptState);
}
UsbIdentity visibleIdentity() { return currentIdentity; }
}

extern "C" {
const uint8_t* __real_tud_descriptor_device_cb(void);
const uint16_t* __real_tud_descriptor_string_cb(uint8_t index, uint16_t language);

const uint8_t* __wrap_tud_descriptor_device_cb(void) {
  static tusb_desc_device_t descriptor;
  descriptor = *reinterpret_cast<const tusb_desc_device_t*>(__real_tud_descriptor_device_cb());
  // Production declares classes per interface. Diagnostics additionally uses
  // an IAD to group CDC at MI_01/02; HID remains MI_00 in either profile.
  descriptor.bDeviceClass = config::DiagnosticSerial ? TUSB_CLASS_MISC : 0;
  descriptor.bDeviceSubClass = config::DiagnosticSerial ? MISC_SUBCLASS_COMMON : 0;
  descriptor.bDeviceProtocol = config::DiagnosticSerial ? MISC_PROTOCOL_IAD : 0;
  if (currentIdentity.valid) {
    descriptor.idVendor = currentIdentity.vid;
    descriptor.idProduct = currentIdentity.pid;
    descriptor.bcdDevice = currentIdentity.deviceVersion;
    descriptor.iManufacturer = currentIdentity.manufacturer.present() ? 1 : 0;
    descriptor.iProduct = currentIdentity.product.present() ? 2 : 0;
    descriptor.iSerialNumber = currentIdentity.serial.present() ? 3 : 0;
  }
  return reinterpret_cast<const uint8_t*>(&descriptor);
}

const uint16_t* __wrap_tud_descriptor_string_cb(uint8_t index, uint16_t language) {
  if (currentIdentity.valid) {
    switch (index) {
      case 0: return currentIdentity.languages.descriptor();
      case 1: return currentIdentity.manufacturer.descriptor();
      case 2: return currentIdentity.product.descriptor();
      case 3: return currentIdentity.serial.descriptor();
      default: break;  // CDC's diagnostic interface string belongs to this firmware.
    }
  }
  return __real_tud_descriptor_string_cb(index, language);
}
}

#include <Adafruit_TinyUSB.h>
#include <hardware/sync.h>
#include "UsbIdentity.h"

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
  // HID comes first, followed by a CDC IAD, so Windows does not bind Logitech's
  // vendor-interface MI_02 driver to the bridge's mouse interface.
  descriptor.bDeviceClass = TUSB_CLASS_MISC;
  descriptor.bDeviceSubClass = MISC_SUBCLASS_COMMON;
  descriptor.bDeviceProtocol = MISC_PROTOCOL_IAD;
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

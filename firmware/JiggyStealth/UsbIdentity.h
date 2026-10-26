#pragma once
#include <stddef.h>
#include <stdint.h>

namespace jiggy_stealth {

// Preserve the raw UTF-16 USB descriptors, including Unicode and an absent
// serial number. Native TinyUSB's ASCII setters otherwise generate a board ID.
struct UsbString {
  uint16_t words[128] = {};
  bool assign(const uint8_t* bytes, size_t available);
  bool present() const { return (words[0] & 255) >= 2 && (words[0] >> 8) == 3; }
  const uint16_t* descriptor() const { return present() ? words : nullptr; }
};

struct UsbIdentity {
  uint16_t vid = 0, pid = 0, deviceVersion = 0;
  uint16_t language = 0x0409;
  bool valid = false, incomplete = false;
  UsbString languages, manufacturer, product, serial;
  bool same(const UsbIdentity& other) const;
};

// Called only on core 0, while the native USB device is detached. The descriptor
// wrappers are linked by the build script, leaving upstream libraries intact.
void setVisibleIdentity(const UsbIdentity& identity);
UsbIdentity visibleIdentity();

}  // namespace jiggy_stealth

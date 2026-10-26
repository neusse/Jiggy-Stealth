#include "UsbIdentity.h"
#include <string.h>

namespace jiggy_stealth {
bool UsbString::assign(const uint8_t* bytes, size_t available) {
  *this = UsbString{};
  if (!bytes || available < 2 || bytes[1] != 3 || bytes[0] < 2 || (bytes[0] & 1) || bytes[0] > available)
    return false;
  for (size_t n = 0; n < bytes[0] / 2u; ++n)
    words[n] = static_cast<uint16_t>(bytes[2 * n] | (uint16_t(bytes[2 * n + 1]) << 8));
  return true;
}

bool UsbIdentity::same(const UsbIdentity& other) const {
  if (valid != other.valid) return false;
  if (!valid) return true;
  return vid == other.vid && pid == other.pid && deviceVersion == other.deviceVersion && language == other.language &&
    memcmp(languages.words, other.languages.words, sizeof(languages.words)) == 0 &&
    memcmp(manufacturer.words, other.manufacturer.words, sizeof(manufacturer.words)) == 0 &&
    memcmp(product.words, other.product.words, sizeof(product.words)) == 0 &&
    memcmp(serial.words, other.serial.words, sizeof(serial.words)) == 0;
}
}  // namespace jiggy_stealth

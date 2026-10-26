#include "HidMouseParser.h"
#include <limits.h>

namespace jiggy_stealth {
namespace {
int32_t signedValue(uint32_t value, unsigned bits) {
  if (bits < 32 && (value & (uint32_t(1) << (bits - 1))))
    value |= ~((uint32_t(1) << bits) - 1);
  return static_cast<int32_t>(value);
}

struct Global {
  uint32_t page = 0, size = 0, count = 0;
  int32_t minimum = 0;
  uint8_t id = 0;
};

struct Local {
  uint32_t usages[32] = {};
  uint32_t minimum = 0, maximum = 0;
  uint8_t count = 0;
  bool hasRange = false, hasMaximum = false;
  uint32_t usage(uint32_t index) const {
    if (count) return usages[index < count ? index : count - 1];
    if (hasRange && hasMaximum && maximum >= minimum) {
      uint32_t width = maximum - minimum;
      return minimum + (index < width ? index : width);
    }
    return 0;
  }
};
}  // namespace

bool HidMouseParser::parse(const uint8_t* descriptor, size_t length) {
  *this = HidMouseParser{};
  if (!descriptor || !length) return false;
  Global global, stack[4];
  Local local;
  bool collections[16] = {}, inMouse = false, foundMouse = false;
  unsigned stackDepth = 0, depth = 0;
  for (size_t cursor = 0; cursor < length;) {
    uint8_t prefix = descriptor[cursor++];
    if (prefix == 0xfe) return false;  // Long items are not needed by this mouse.
    unsigned size = prefix & 3;
    if (size == 3) size = 4;
    if (cursor + size > length) return false;
    uint32_t value = 0;
    for (unsigned n = 0; n < size; ++n) value |= uint32_t(descriptor[cursor++]) << (8 * n);
    unsigned type = (prefix >> 2) & 3, tag = prefix >> 4;
    if (type == 1) {
      switch (tag) {
        case 0: global.page = value; break;
        case 1: global.minimum = size ? signedValue(value, size * 8) : 0; break;
        case 7: global.size = value; break;
        case 8:
          if (!value || value > 255) return false;
          global.id = static_cast<uint8_t>(value);
          usesIds_ = true;
          break;
        case 9: global.count = value; break;
        case 10:
          if (stackDepth == 4) return false;
          stack[stackDepth++] = global;
          break;
        case 11:
          if (!stackDepth) return false;
          global = stack[--stackDepth];
          break;
        default: break;
      }
    } else if (type == 2) {
      uint32_t usage = size == 4 ? value : ((global.page << 16) | value);
      if (tag == 0) {
        if (local.count == 32) return false;
        local.usages[local.count++] = usage;
      } else if (tag == 1) {
        local.minimum = usage;
        local.hasRange = true;
      } else if (tag == 2) {
        local.maximum = usage;
        local.hasMaximum = true;
      } else if (tag == 10) {
        return false;  // Delimiter alternatives need a different decoder.
      }
    } else if (type == 0) {
      if (tag == 10) {  // Collection
        if (depth == 16) return false;
        collections[depth++] = inMouse;
        inMouse = inMouse || local.usage(0) == 0x00010002;
      } else if (tag == 12) {  // End Collection
        if (!depth) return false;
        inMouse = collections[--depth];
      } else if (tag == 8) {  // Input: constants and unrelated fields still consume bits.
        if (!global.size || global.size > 32 || !global.count || global.count > 512 ||
            global.size * global.count > 512) return false;
        Report* report = nullptr;
        for (uint8_t n = 0; n < count_; ++n)
          if (reports_[n].id == global.id) report = &reports_[n];
        if (!report) {
          if (count_ == 16) return false;
          report = &reports_[count_++];
          report->id = global.id;
        }
        if (report->bits + global.size * global.count > 512) return false;
        const bool dataVariable = (value & 3) == 2;
        const bool relative = (value & 4) != 0;
        if (dataVariable) {
          for (uint32_t n = 0; n < global.count; ++n) {
            const uint32_t usage = local.usage(n);
            const uint32_t page = usage >> 16, code = usage & 0xffff;
            Field field;
            bool recognized = false;
            if (inMouse && page == 9 && code >= 1 && code <= 16) {
              field.kind = Kind::Button;
              field.button = static_cast<uint8_t>(code - 1);
              recognized = true;
            } else if (inMouse && page == 1 && relative) {
              if (code == 0x30) { field.kind = Kind::X; recognized = true; }
              if (code == 0x31) { field.kind = Kind::Y; recognized = true; }
              if (code == 0x38) { field.kind = Kind::Wheel; recognized = true; }
            } else if (page == 0x0c && code == 0x238 && relative) {
              field.kind = Kind::Pan;
              recognized = true;
            }
            if (recognized) {
              if (report->count == 24) return false;
              field.offset = static_cast<uint16_t>(report->bits + n * global.size);
              field.bits = static_cast<uint8_t>(global.size);
              field.isSigned = global.minimum < 0;
              report->fields[report->count++] = field;
              foundMouse = true;
            }
          }
        }
        report->bits = static_cast<uint16_t>(report->bits + global.size * global.count);
      }
      local = Local{};  // HID local state expires after every main item.
    }
  }
  if (depth || stackDepth) return false;
  if (usesIds_) {
    for (uint8_t n = 0; n < count_; ++n) if (!reports_[n].id) return false;
  }
  supported_ = foundMouse;
  return true;
}

DecodeResult HidMouseParser::decode(const uint8_t* data, size_t length, MouseInput& result) const {
  result = MouseInput{};
  if (!data || !length) return DecodeResult::Malformed;
  uint8_t id = 0;
  if (usesIds_) { id = *data++; --length; }
  const Report* report = nullptr;
  for (uint8_t n = 0; n < count_; ++n)
    if (reports_[n].id == id) report = &reports_[n];
  if (!report || !report->count) return DecodeResult::Ignored;
  if (length * 8 < report->bits) return DecodeResult::Malformed;
  for (uint8_t n = 0; n < report->count; ++n) {
    const Field& field = report->fields[n];
    uint32_t raw = 0;
    for (unsigned b = 0; b < field.bits; ++b) {
      unsigned bit = field.offset + b;
      raw |= uint32_t((data[bit / 8] >> (bit % 8)) & 1) << b;
    }
    int32_t value = field.isSigned ? signedValue(raw, field.bits)
                                 : static_cast<int32_t>(raw > INT32_MAX ? INT32_MAX : raw);
    switch (field.kind) {
      case Kind::Button:
        result.buttonMask |= uint16_t(1) << field.button;
        if (raw) result.buttons |= uint16_t(1) << field.button;
        break;
      case Kind::X: result.x = value; break;
      case Kind::Y: result.y = value; break;
      case Kind::Wheel: result.wheel = value; break;
      case Kind::Pan: result.pan = value; break;
    }
  }
  return DecodeResult::Mouse;
}

bool HidMouseParser::has(Kind kind) const {
  for (uint8_t r = 0; r < count_; ++r)
    for (uint8_t f = 0; f < reports_[r].count; ++f)
      if (reports_[r].fields[f].kind == kind) return true;
  return false;
}
bool HidMouseParser::hasWheel() const { return has(Kind::Wheel); }
bool HidMouseParser::hasPan() const { return has(Kind::Pan); }
}  // namespace jiggy_stealth

#pragma once
#include <stddef.h>
#include <stdint.h>

namespace jiggy_stealth {

struct MouseInput {
  uint16_t buttons = 0;
  uint16_t buttonMask = 0;
  int32_t x = 0, y = 0, wheel = 0, pan = 0;
};

enum class DecodeResult { Ignored, Mouse, Malformed };

// Bounded HID short-item parser for relative mouse fields. Offsets exclude the
// report-ID prefix. Keyboard/vendor reports are tracked but never forwarded.
class HidMouseParser {
 public:
  bool parse(const uint8_t* descriptor, size_t length);
  DecodeResult decode(const uint8_t* report, size_t length, MouseInput& result) const;
  bool supported() const { return supported_; }
  bool usesReportIds() const { return usesIds_; }
  bool hasWheel() const;
  bool hasPan() const;

 private:
  enum class Kind : uint8_t { Button, X, Y, Wheel, Pan };
  struct Field {
    uint16_t offset = 0;
    uint8_t bits = 0, button = 0;
    Kind kind = Kind::X;
    bool isSigned = false;
  };
  struct Report {
    uint16_t bits = 0;
    uint8_t id = 0, count = 0;
    Field fields[24] = {};
  };
  Report reports_[16] = {};
  uint8_t count_ = 0;
  bool supported_ = false, usesIds_ = false;
  bool has(Kind kind) const;
};

}  // namespace jiggy_stealth

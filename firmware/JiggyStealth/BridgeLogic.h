#pragma once
#include "Config.h"
#include <stdint.h>

namespace jiggy_stealth {

enum class Mode : uint8_t { Movement, Shift };
enum class ButtonEvent { None, ShortPress, LongPress };

class Button {
 public:
  void begin(uint32_t now, bool pressed);
  ButtonEvent update(uint32_t now, bool pressed);
 private:
  uint32_t changedAt_ = 0, pressedAt_ = 0;
  bool raw_ = false, stable_ = false, longSent_ = false, awaitRelease_ = false;
};

enum class ActionKind { None, MoveOut, MoveBack, ShiftDown, ShiftUp };
struct Action {
  ActionKind kind = ActionKind::None;
  int16_t x = 0, y = 0;
};

// Only the device core owns this state. An action changes state only after the
// USB endpoint accepts it; a busy endpoint never consumes a key release.
class Activity {
 public:
  void begin(uint32_t now, uint32_t seed);
  void toggle(uint32_t now);
  void changeMode();
  void stop();
  void physical(uint32_t now, uint16_t buttons);
  Action next(uint32_t now, bool quiet, bool connected) const;
  void accepted(const Action& action, uint32_t now);
  bool enabled() const { return enabled_; }
  Mode mode() const { return mode_; }
  bool shiftDown() const { return shiftDown_; }
  uint32_t nextAt() const { return nextAt_; }
 private:
  void schedule(uint32_t now);
  uint32_t random();
  Mode mode_ = Mode::Movement;
  bool enabled_ = false, returning_ = false, shiftDown_ = false, release_ = false;
  uint16_t buttons_ = 0;
  uint32_t random_ = 1, lastPhysical_ = 0, nextAt_ = 0, returnAt_ = 0, releaseAt_ = 0;
  int16_t x_ = config::MovementCounts, y_ = 0;
};

struct MouseEvent {
  uint32_t epoch = 0;
  uint16_t buttons = 0;
  int32_t x = 0, y = 0, wheel = 0, pan = 0;
};

#pragma pack(push, 1)
struct MouseReport {
  uint16_t buttons = 0;
  int16_t x = 0, y = 0;
  int8_t wheel = 0, pan = 0;
};
#pragma pack(pop)
static_assert(sizeof(MouseReport) == 8, "The HID descriptor uses eight payload bytes.");

class Forwarder {
 public:
  void load(const MouseEvent& event) { pending_ = event; active_ = true; }
  void clear() { pending_ = MouseEvent{}; active_ = false; }
  bool active() const { return active_; }
  MouseReport report() const;
  void accepted(const MouseReport& report);
 private:
  MouseEvent pending_;
  bool active_ = false;
};

}  // namespace jiggy_stealth

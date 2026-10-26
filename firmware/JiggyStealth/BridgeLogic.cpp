#include "BridgeLogic.h"

namespace jiggy_stealth {
namespace {
bool due(uint32_t now, uint32_t deadline) { return static_cast<int32_t>(now - deadline) >= 0; }
int32_t limit(int32_t value, int32_t magnitude) {
  return value > magnitude ? magnitude : (value < -magnitude ? -magnitude : value);
}
}

void Button::begin(uint32_t now, bool pressed) {
  *this = Button{};
  changedAt_ = now;
  raw_ = stable_ = pressed;
  awaitRelease_ = pressed;
}

ButtonEvent Button::update(uint32_t now, bool pressed) {
  if (pressed != raw_) { raw_ = pressed; changedAt_ = now; }
  if (now - changedAt_ >= config::DebounceMs && stable_ != raw_) {
    stable_ = raw_;
    if (stable_) { pressedAt_ = now; longSent_ = false; }
    else {
      if (awaitRelease_) { awaitRelease_ = false; return ButtonEvent::None; }
      if (!longSent_) return ButtonEvent::ShortPress;
    }
  }
  if (stable_ && raw_ && !awaitRelease_ && !longSent_ && now - pressedAt_ >= config::LongPressMs) {
    longSent_ = true;
    return ButtonEvent::LongPress;
  }
  return ButtonEvent::None;
}

void Activity::begin(uint32_t now, uint32_t seed) {
  *this = Activity{};
  random_ = seed ? seed : 1;
  lastPhysical_ = now;
  schedule(now);
}

uint32_t Activity::random() {
  random_ ^= random_ << 13;
  random_ ^= random_ >> 17;
  random_ ^= random_ << 5;
  return random_;
}

void Activity::schedule(uint32_t now) {
  nextAt_ = now + config::IntervalMinMs + random() % (config::IntervalMaxMs - config::IntervalMinMs + 1);
  unsigned direction = random() & 3;
  x_ = direction < 2 ? (direction ? -config::MovementCounts : config::MovementCounts) : 0;
  y_ = direction >= 2 ? (direction == 2 ? config::MovementCounts : -config::MovementCounts) : 0;
}

void Activity::toggle(uint32_t now) {
  if (enabled_) { stop(); return; }
  enabled_ = true;
  lastPhysical_ = now;
  returning_ = false;
  schedule(now);
}

void Activity::stop() {
  enabled_ = false;
  returning_ = false;
  if (shiftDown_) release_ = true;
}

void Activity::changeMode() {
  stop();
  mode_ = mode_ == Mode::Movement ? Mode::Shift : Mode::Movement;
}

void Activity::physical(uint32_t now, uint16_t buttons) {
  lastPhysical_ = now;
  buttons_ = buttons;
  returning_ = false;
  if (shiftDown_) release_ = true;
  schedule(now);
}

Action Activity::next(uint32_t now, bool quiet, bool connected) const {
  if (shiftDown_) {
    if (release_ || due(now, releaseAt_)) return {ActionKind::ShiftUp, 0, 0};
    return {};
  }
  if (!enabled_ || !connected || !quiet || buttons_) return {};
  if (returning_) return due(now, returnAt_) ? Action{ActionKind::MoveBack, static_cast<int16_t>(-x_), static_cast<int16_t>(-y_)} : Action{};
  if (now - lastPhysical_ < config::IdleMs || !due(now, nextAt_)) return {};
  return mode_ == Mode::Movement ? Action{ActionKind::MoveOut, x_, y_} : Action{ActionKind::ShiftDown, 0, 0};
}

void Activity::accepted(const Action& action, uint32_t now) {
  switch (action.kind) {
    case ActionKind::MoveOut:
      returning_ = true;
      returnAt_ = now + config::ReturnDelayMs;
      break;
    case ActionKind::MoveBack:
      returning_ = false;
      schedule(now);
      break;
    case ActionKind::ShiftDown:
      shiftDown_ = true;
      release_ = false;
      releaseAt_ = now + config::ShiftHoldMs;
      break;
    case ActionKind::ShiftUp:
      shiftDown_ = release_ = false;
      schedule(now);
      break;
    default: break;
  }
}

MouseReport Forwarder::report() const {
  MouseReport result;
  result.buttons = pending_.buttons;
  result.x = static_cast<int16_t>(limit(pending_.x, 32767));
  result.y = static_cast<int16_t>(limit(pending_.y, 32767));
  result.wheel = static_cast<int8_t>(limit(pending_.wheel, 127));
  result.pan = static_cast<int8_t>(limit(pending_.pan, 127));
  return result;
}

void Forwarder::accepted(const MouseReport& report) {
  pending_.x -= report.x;
  pending_.y -= report.y;
  pending_.wheel -= report.wheel;
  pending_.pan -= report.pan;
  if (!pending_.x && !pending_.y && !pending_.wheel && !pending_.pan) active_ = false;
}

}  // namespace jiggy_stealth

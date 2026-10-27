#include "BridgeLogic.h"
#include "HidMouseParser.h"
#include "HidDescriptors.h"
#include "M305Descriptor.h"
#include "UsbIdentity.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace jiggy_stealth;
static unsigned checks = 0;
#define CHECK(condition) do { ++checks; if (!(condition)) { std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); std::exit(1); } } while (0)

std::array<uint8_t, 8> m305(int x, int y, uint16_t buttons = 0, int8_t wheel = 0, int8_t pan = 0) {
  uint16_t ux = static_cast<uint16_t>(x) & 0xfff, uy = static_cast<uint16_t>(y) & 0xfff;
  return {2, static_cast<uint8_t>(buttons), static_cast<uint8_t>(buttons >> 8),
          static_cast<uint8_t>(ux), static_cast<uint8_t>((ux >> 8) | (uy << 4)),
          static_cast<uint8_t>(uy >> 4), static_cast<uint8_t>(wheel), static_cast<uint8_t>(pan)};
}

void receiverReports() {
  HidMouseParser parser;
  CHECK(parser.parse(M305Descriptor, sizeof(M305Descriptor)));
  CHECK(parser.supported() && parser.usesReportIds() && parser.hasWheel() && parser.hasPan());
  MouseInput result;
  for (int x = -2047; x <= 2047; ++x) {
    auto packet = m305(x, -x, 0x8005, -3, 4);
    CHECK(parser.decode(packet.data(), packet.size(), result) == DecodeResult::Mouse);
    CHECK(result.x == x && result.y == -x);
    CHECK(result.buttons == 0x8005 && result.buttonMask == 0xffff);
    CHECK(result.wheel == -3 && result.pan == 4);
  }
  auto scrollOnly = m305(0, 0, 0, 1, -1);
  CHECK(parser.decode(scrollOnly.data(), scrollOnly.size(), result) == DecodeResult::Mouse);
  CHECK(!result.x && !result.y && result.wheel == 1 && result.pan == -1);
  auto packet = m305(0, 0, 1);
  CHECK(parser.decode(packet.data(), 7, result) == DecodeResult::Malformed);
  CHECK(parser.decode(nullptr, 8, result) == DecodeResult::Malformed);
  packet[0] = 3; // Consumer-control report shares this interface; never reinterpret as mouse.
  CHECK(parser.decode(packet.data(), packet.size(), result) == DecodeResult::Ignored);
  packet[0] = 8;
  CHECK(parser.decode(packet.data(), packet.size(), result) == DecodeResult::Ignored);
  packet[0] = 99;
  CHECK(parser.decode(packet.data(), packet.size(), result) == DecodeResult::Ignored);
  CHECK(!parser.parse(M305Descriptor, sizeof(M305Descriptor) - 1));
  const uint8_t malformed[] = {0x05};
  CHECK(!parser.parse(malformed, sizeof(malformed)));
  const uint8_t badPop[] = {0xb4};
  CHECK(!parser.parse(badPop, sizeof(badPop)));
  CHECK(!parser.parse(nullptr, 0));
  std::puts("PASS captured receiver: packed 12-bit signed axes, 16 buttons, wheel, tilt, IDs, malformed input");
}

void outputReports() {
  HidMouseParser parser;
  CHECK(parser.parse(HidDescriptor.data(), HidDescriptor.size()) && parser.supported());
  Forwarder forwarder;
  MouseEvent event;
  event.buttons = 0x8001;
  event.x = 100000;
  event.y = -100000;
  event.wheel = 500;
  event.pan = -500;
  forwarder.load(event);
  int32_t x = 0, y = 0, wheel = 0, pan = 0;
  unsigned packets = 0;
  while (forwarder.active()) {
    CHECK(++packets < 10);
    MouseReport report = forwarder.report();
    std::array<uint8_t, 9> bytes{};
    bytes[0] = config::MouseReportId;
    std::memcpy(bytes.data() + 1, &report, sizeof(report));
    MouseInput decoded;
    CHECK(parser.decode(bytes.data(), bytes.size(), decoded) == DecodeResult::Mouse);
    CHECK(decoded.buttons == event.buttons);
    x += decoded.x; y += decoded.y; wheel += decoded.wheel; pan += decoded.pan;
    forwarder.accepted(report);
  }
  CHECK(x == event.x && y == event.y && wheel == event.wheel && pan == event.pan);
  event = MouseEvent{};
  event.buttons = 1;
  forwarder.load(event);
  CHECK(forwarder.report().buttons == 1);
  forwarder.accepted(forwarder.report());
  event.buttons = 0;
  forwarder.load(event);
  CHECK(forwarder.active() && !forwarder.report().buttons);
  forwarder.accepted(forwarder.report());
  CHECK(!forwarder.active());
  std::puts("PASS output descriptor and forwarding: wide deltas split without loss, scroll/pan and button releases");
}

void buttonBehavior() {
  Button button;
  button.begin(0, false);
  CHECK(button.update(10, true) == ButtonEvent::None);
  CHECK(button.update(15, false) == ButtonEvent::None);
  CHECK(button.update(20, true) == ButtonEvent::None);
  CHECK(button.update(46, true) == ButtonEvent::None);
  CHECK(button.update(80, false) == ButtonEvent::None);
  CHECK(button.update(106, false) == ButtonEvent::ShortPress);
  CHECK(button.update(120, false) == ButtonEvent::None);
  CHECK(button.update(200, true) == ButtonEvent::None);
  CHECK(button.update(226, true) == ButtonEvent::None);
  CHECK(button.update(2225, true) == ButtonEvent::None);
  CHECK(button.update(2226, true) == ButtonEvent::LongPress);
  CHECK(button.update(3000, true) == ButtonEvent::None);
  CHECK(button.update(3100, false) == ButtonEvent::None);
  CHECK(button.update(3126, false) == ButtonEvent::None);
  button.begin(0xfffffff0u, true); // Boot while held must not enable on release.
  CHECK(button.update(5000, true) == ButtonEvent::None);
  CHECK(button.update(5010, false) == ButtonEvent::None);
  CHECK(button.update(5040, false) == ButtonEvent::None);
  std::puts("PASS button: debounce, short toggle, one long event, no short event after long or boot-held release");
}

void activityBehavior() {
  Activity a;
  a.begin(0, 123);
  CHECK(!a.enabled() && a.mode() == Mode::Movement);
  CHECK(a.next(100000, true, true).kind == ActionKind::None);
  a.toggle(100);
  CHECK(a.enabled());
  CHECK(a.nextAt() >= 30100 && a.nextAt() <= 60100);
  CHECK(a.next(a.nextAt() - 1, true, true).kind == ActionKind::None);
  uint32_t now = a.nextAt();
  CHECK(a.next(now, false, true).kind == ActionKind::None);
  CHECK(a.next(now, true, false).kind == ActionKind::None);
  Action out = a.next(now, true, true);
  CHECK(out.kind == ActionKind::MoveOut && (out.x || out.y));
  CHECK(std::abs(out.x) + std::abs(out.y) == config::MovementCounts);
  // A rejected send does not advance the action or lose the return.
  CHECK(a.next(now + 1, true, true).kind == ActionKind::MoveOut);
  a.accepted(out, now);
  CHECK(a.next(now + 99, true, true).kind == ActionKind::None);
  Action back = a.next(now + 100, true, true);
  CHECK(back.kind == ActionKind::MoveBack && back.x == -out.x && back.y == -out.y);
  a.physical(now + 100, 0);
  CHECK(a.next(now + 100, true, true).kind == ActionKind::None); // Real input cancels pending return.
  now = a.nextAt();
  out = a.next(now, true, true);
  a.accepted(out, now);
  a.toggle(now + 1);
  CHECK(!a.enabled() && a.next(now + 100, true, true).kind == ActionKind::None);
  a.toggle(now + 200);
  a.physical(now + 210, 1);
  CHECK(a.next(a.nextAt() + 100, true, true).kind == ActionKind::None); // Held button blocks injection.
  a.physical(now + 100000, 0);
  CHECK(a.next(now + 100001, true, true).kind == ActionKind::None);

  a.changeMode();
  CHECK(!a.enabled() && a.mode() == Mode::Shift);
  a.toggle(now);
  uint32_t downAt = a.nextAt();
  Action down = a.next(downAt, true, true);
  CHECK(down.kind == ActionKind::ShiftDown && !a.shiftDown());
  a.accepted(down, downAt);
  CHECK(a.shiftDown());
  CHECK(a.next(downAt + 29, true, true).kind == ActionKind::None);
  Action up = a.next(downAt + 30, false, false);
  CHECK(up.kind == ActionKind::ShiftUp); // Release is higher priority than queued input or link loss.
  a.stop();
  CHECK(a.next(downAt + 1, false, false).kind == ActionKind::ShiftUp);
  // Simulate a busy endpoint through suspend: release stays pending until accepted.
  CHECK(a.next(downAt + 100000, true, true).kind == ActionKind::ShiftUp);
  a.accepted(up, downAt + 100000);
  CHECK(!a.shiftDown() && !a.enabled());
  CHECK(a.next(downAt + 200000, true, true).kind == ActionKind::None);

  // Changing the descriptor mode must stop activity without losing a held
  // Shift's release while the old endpoint is busy or suspended.
  a.toggle(downAt + 200001);
  downAt = a.nextAt();
  down = a.next(downAt, true, true);
  CHECK(down.kind == ActionKind::ShiftDown);
  a.accepted(down, downAt);
  a.changeMode();
  CHECK(a.mode() == Mode::Movement && !a.enabled() && a.shiftDown());
  up = a.next(downAt + 1, false, false);
  CHECK(up.kind == ActionKind::ShiftUp);
  a.accepted(up, downAt + config::ModeKeyReleaseMs);
  CHECK(!a.shiftDown() && !a.enabled());
  CHECK(a.next(downAt + 200000, true, true).kind == ActionKind::None);

  a.begin(0xfffff000u, 987);
  a.toggle(0xfffff000u);
  uint32_t wrapped = a.nextAt();
  CHECK(a.next(wrapped - 1, true, true).kind == ActionKind::None);
  CHECK(a.next(wrapped, true, true).kind == ActionKind::MoveOut);
  uint32_t previous = wrapped;
  for (unsigned i = 0; i < 10000; ++i) {
    a.physical(previous, 0);
    uint32_t interval = a.nextAt() - previous;
    CHECK(interval >= 30000 && interval <= 60000);
    previous += interval;
  }
  std::puts("PASS activity: startup off/movement default, idle gating, priorities, cancellation, reliable Shift up and timer wrap");
}

void identityBehavior() {
  UsbIdentity receiver;
  receiver.valid = true;
  receiver.vid = 0x046d;
  receiver.pid = 0xc52b;
  receiver.deviceVersion = 0x1201;
  const uint8_t name[] = {18,3,'L',0,'o',0,'g',0,'i',0,'t',0,'e',0,'c',0,'h',0};
  CHECK(receiver.manufacturer.assign(name, sizeof(name)));
  CHECK(receiver.manufacturer.present() && !receiver.serial.present());
  CHECK(std::memcmp(receiver.manufacturer.descriptor(), name, sizeof(name)) == 0);
  UsbIdentity same = receiver;
  CHECK(receiver.same(same));
  ++same.pid;
  CHECK(!receiver.same(same));
  same = receiver;
  const uint8_t serial[] = {4,3,'1',0};
  CHECK(same.serial.assign(serial, sizeof(serial)));
  CHECK(!receiver.same(same));
  const uint8_t unicode[] = {10,3,0x2d,0x4e,0x87,0x65,0x3d,0xd8,0x00,0xde};
  CHECK(receiver.product.assign(unicode, sizeof(unicode)));
  CHECK(std::memcmp(receiver.product.descriptor(), unicode, sizeof(unicode)) == 0);
  CHECK(!receiver.product.assign(unicode, sizeof(unicode) - 1));
  CHECK(!receiver.product.present());
  const uint8_t odd[] = {3,3,0};
  CHECK(!receiver.product.assign(odd, sizeof(odd)));
  const uint8_t wrongType[] = {4,1,0,0};
  CHECK(!receiver.product.assign(wrongType, sizeof(wrongType)));
  CHECK(!receiver.product.assign(nullptr, 0));
  CHECK(UsbIdentity{}.same(UsbIdentity{}));
  std::array<uint8_t,254> longest{};
  longest[0] = 254;
  longest[1] = 3;
  for (unsigned n = 2; n < longest.size(); n += 2) longest[n] = 'A';
  CHECK(receiver.product.assign(longest.data(), longest.size()));
  CHECK(std::memcmp(receiver.product.descriptor(), longest.data(), longest.size()) == 0);
  std::puts("PASS donor identity: VID/PID matching, raw Unicode strings, absent serial and malformed/max-length descriptors");
}

unsigned usbDeviceBehavior();

int main() {
  receiverReports();
  outputReports();
  buttonBehavior();
  activityBehavior();
  identityBehavior();
  checks += usbDeviceBehavior();
  std::printf("All %u checks passed.\n", checks);
}

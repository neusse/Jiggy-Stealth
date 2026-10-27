#include <pio_usb.h>
#include <Adafruit_TinyUSB.h>
#include <Adafruit_NeoPixel.h>
#include <pico/util/queue.h>
#include <pico/critical_section.h>
#include <hardware/clocks.h>
#include <stdarg.h>
#include "HidMouseParser.h"
#include "BridgeLogic.h"
#include "HidDescriptors.h"
#include "UsbIdentity.h"
#include "UsbDevice.h"

#if !defined(ARDUINO_ADAFRUIT_FEATHER_RP2040_USB_HOST)
#error Select Adafruit Feather RP2040 USB Host, not a generic Pico or Feather.
#endif
#if F_CPU != 120000000 && F_CPU != 240000000
#error PIO USB requires a supported clock. Select 120 MHz.
#endif

using namespace jiggy_stealth;
// Avoid splitting the default 8 KB stack between the two USB stacks.
bool core1_separate_stack = true;
Adafruit_USBH_Host USBHost;
Adafruit_USBD_HID hid(MouseHidDescriptor, sizeof(MouseHidDescriptor), HID_ITF_PROTOCOL_NONE, 1, false);
Adafruit_NeoPixel pixel(1, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);
queue_t mouseQueue;
critical_section_t sharedLock;
volatile bool coreReady = false;
#if JIGGY_DIAGNOSTICS
queue_t rawQueue;
volatile bool rawEnabled = false;
#endif

struct InterfaceInfo {
  uint16_t vid = 0, pid = 0, length = 0;
  uint8_t address = 0, instance = 0, protocol = 0;
  bool supported = false, wheel = false, pan = false;
  uint8_t descriptor[config::DescriptorCapacity] = {};
};
struct HostSlot {
  HidMouseParser parser;
  uint8_t address = 0, instance = 0;
  bool supported = false;
  uint16_t buttons = 0;
};
struct SharedState {
  uint32_t epoch = 0, sequence = 0, lastPhysical = 0;
  uint32_t reports = 0, events = 0, overflows = 0, decodeErrors = 0, rawDrops = 0;
  uint16_t buttons = 0;
  uint8_t connected = 0, supported = 0;
  bool ready = false, hostFault = false;
  uint32_t identityRevision = 0;
};
#if JIGGY_DIAGNOSTICS
struct RawPacket {
  uint8_t address = 0, instance = 0, length = 0;
  uint8_t bytes[64] = {};
};
#endif
HostSlot hostSlots[config::InterfaceCapacity];  // Core 1 only.
InterfaceInfo interfaceInfo[config::InterfaceCapacity];  // sharedLock protects these.
SharedState shared;
UsbIdentity discoveredIdentity, pendingIdentity;
struct IdentityJob {
  UsbIdentity identity;
  tusb_desc_device_t device = {};
  uint16_t buffer[128] = {};
  uint32_t generation = 0, stageAt = 0;
  uint8_t address = 0, stage = 0;
  bool busy = false, complete = false;
};
IdentityJob identityJob;  // Core 1 owns asynchronous descriptor requests.
uint32_t seenIdentityRevision = 0, identitySwitchAt = 0, startupAt = 0;
bool identityAttached = false, identityChanging = false;
bool deviceConfigured = false, modeDisconnectPending = false;
uint32_t modeReleaseAt = 0;
Activity activity;
Button button;
Forwarder forwarder;
uint32_t seenEpoch = 0, seenSequence = 0, seenOverflows = 0;
uint32_t physicalSent = 0, movementsSent = 0, shiftPresses = 0, shiftReleases = 0;
volatile uint16_t lastButtonsSent = 0;
volatile uint8_t keyboardModifier = 0;
bool wasUsbActive = false, neutralKeyboard = false, syncMouse = true, pixelReady = false;
uint32_t lastColor = 0xffffffffu;
SharedState hostSnapshot() {
  critical_section_enter_blocking(&sharedLock);
  SharedState state = shared;
  critical_section_exit(&sharedLock);
  return state;
}

#if JIGGY_DIAGNOSTICS
char diagnostic[8192];
size_t diagnosticLength = 0, diagnosticPosition = 0;

void appendDiagnostic(const char* format, ...) {
  if (diagnosticLength >= sizeof(diagnostic) - 1) return;
  va_list args;
  va_start(args, format);
  int written = vsnprintf(diagnostic + diagnosticLength, sizeof(diagnostic) - diagnosticLength, format, args);
  va_end(args);
  if (written > 0) diagnosticLength += min(static_cast<size_t>(written), sizeof(diagnostic) - diagnosticLength - 1);
}

void startDiagnostic() { diagnosticLength = diagnosticPosition = 0; }

void statusDiagnostic() {
  SharedState s = hostSnapshot();
  startDiagnostic();
  appendDiagnostic("{\"version\":\"%s\",\"build\":\"%s\",\"cdc_enabled\":true,\"enabled\":%s,\"mode\":\"%s\",\"usb_active\":%s,\"host_ready\":%s,\"interfaces\":%u,\"mouse_interfaces\":%u,\"buttons\":%u,\"reports\":%lu,\"events\":%lu,\"forwarded\":%lu,\"overflows\":%lu,\"decode_errors\":%lu,\"movements\":%lu,\"shift_presses\":%lu,\"shift_releases\":%lu,\"shift_down\":%s,\"host_fault\":%s,\"raw_drops\":%lu",
    config::Version, config::BuildProfile, activity.enabled() ? "true" : "false", activity.mode() == Mode::Movement ? "movement" : "shift",
    wasUsbActive ? "true" : "false", s.ready ? "true" : "false", s.connected, s.supported, s.buttons,
    static_cast<unsigned long>(s.reports), static_cast<unsigned long>(s.events), static_cast<unsigned long>(physicalSent),
    static_cast<unsigned long>(s.overflows), static_cast<unsigned long>(s.decodeErrors), static_cast<unsigned long>(movementsSent),
    static_cast<unsigned long>(shiftPresses), static_cast<unsigned long>(shiftReleases), activity.shiftDown() ? "true" : "false",
    s.hostFault ? "true" : "false", static_cast<unsigned long>(s.rawDrops));
  UsbIdentity identity = visibleIdentity();
  appendDiagnostic(",\"identity_forwarded\":%s,\"usb_vid\":\"%04x\",\"usb_pid\":\"%04x\",\"identity_incomplete\":%s,\"keyboard_hid\":%s,\"usb_reconfiguring\":%s}\n",
    identity.valid ? "true" : "false", identity.valid ? identity.vid : 0x239a,
    identity.valid ? identity.pid : 0xcafe, identity.incomplete ? "true" : "false",
    keyboardHidEnabled() ? "true" : "false", identityChanging || modeDisconnectPending ? "true" : "false");
}

void descriptorDiagnostic() {
  InterfaceInfo snapshots[config::InterfaceCapacity];
  critical_section_enter_blocking(&sharedLock);
  memcpy(snapshots, interfaceInfo, sizeof(snapshots));
  critical_section_exit(&sharedLock);
  startDiagnostic();
  for (const auto& item : snapshots) {
    if (!item.address) continue;
    appendDiagnostic("HID addr=%u instance=%u vid=%04x pid=%04x protocol=%u supported=%u wheel=%u pan=%u length=%u\n",
      item.address, item.instance, item.vid, item.pid, item.protocol, item.supported, item.wheel, item.pan, item.length);
    for (uint16_t n = 0; n < item.length; ++n) appendDiagnostic("%02x%s", item.descriptor[n], (n + 1) % 32 ? " " : "\n");
    appendDiagnostic("\n");
  }
}

void diagnosticsLoop() {
  // Every write is bounded by free CDC space. Diagnostics cannot stall forwarding.
  if (diagnosticPosition < diagnosticLength && Serial) {
    size_t available = static_cast<size_t>(max(Serial.availableForWrite(), 0));
    size_t amount = min(min(available, size_t(64)), diagnosticLength - diagnosticPosition);
    if (amount) diagnosticPosition += Serial.write(reinterpret_cast<uint8_t*>(diagnostic) + diagnosticPosition, amount);
  }
  if (diagnosticPosition != diagnosticLength) return;
  if (Serial.available()) {
    switch (Serial.read()) {
      case 's': statusDiagnostic(); break;
      case 'd': descriptorDiagnostic(); break;
      case 'r': __atomic_store_n(&rawEnabled, true, __ATOMIC_RELEASE); break;
      case 'q': __atomic_store_n(&rawEnabled, false, __ATOMIC_RELEASE); break;
      case '?':
        startDiagnostic();
        appendDiagnostic("s=status JSON; d=descriptors; r=raw mouse reports on; q=raw off. Activity requires the physical button.\n");
        break;
      default: break;
    }
  } else {
    RawPacket packet;
    if (queue_try_remove(&rawQueue, &packet) && Serial) {
      startDiagnostic();
      appendDiagnostic("RAW addr=%u instance=%u length=%u ", packet.address, packet.instance, packet.length);
      for (uint8_t n = 0; n < packet.length; ++n) appendDiagnostic("%02x ", packet.bytes[n]);
      appendDiagnostic("\n");
    }
  }
}
#endif

uint16_t getReport(uint8_t id, hid_report_type_t type, uint8_t* buffer, uint16_t requested) {
  if (type != HID_REPORT_TYPE_INPUT || !usbReportAllowed(id)) return 0;
  uint8_t bytes[8] = {};
  if (id == config::MouseReportId) {
    bytes[0] = static_cast<uint8_t>(lastButtonsSent);
    bytes[1] = static_cast<uint8_t>(lastButtonsSent >> 8);
  } else if (id == config::KeyboardReportId) bytes[0] = keyboardModifier;
  else return 0;
  uint16_t length = min(requested, static_cast<uint16_t>(sizeof(bytes)));
  memcpy(buffer, bytes, length);
  return length;
}
void setReport(uint8_t, hid_report_type_t, const uint8_t*, uint16_t) {}

void setup() {
  hid.setReportCallback(getReport, setReport);
  deviceConfigured = configureUsbDevice(hid);
  critical_section_init(&sharedLock);
  queue_init(&mouseQueue, sizeof(MouseEvent), config::QueueCapacity);
#if JIGGY_DIAGNOSTICS
  queue_init(&rawQueue, sizeof(RawPacket), 16);
#endif
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(NEOPIXEL_POWER, OUTPUT);
  digitalWrite(NEOPIXEL_POWER, HIGH);
  pinMode(LED_BUILTIN, OUTPUT);
  button.begin(millis(), digitalRead(PIN_BUTTON) == LOW);
  activity.begin(millis(), micros() ^ 0x3052040u);
  startupAt = millis();
  // Native USB remains detached while the host discovers the donor identity.
  __atomic_store_n(&coreReady, true, __ATOMIC_RELEASE);
}

void resynchronize(const SharedState& s) {
  activity.stop();
  activity.physical(millis(), s.buttons);
  forwarder.clear();
  MouseEvent discarded;
  while (queue_try_remove(&mouseQueue, &discarded)) {}
  neutralKeyboard = keyboardHidEnabled();
  syncMouse = true;
}

bool sendMouse(const MouseReport& report) {
  if (!hid.sendReport(config::MouseReportId, &report, sizeof(report))) return false;
  lastButtonsSent = report.buttons;
  return true;
}

bool sendKeyboard(uint8_t modifier) {
  if (!usbReportAllowed(config::KeyboardReportId)) return false;
  uint8_t keys[8] = {modifier, 0, 0, 0, 0, 0, 0, 0};
  if (!hid.sendReport(config::KeyboardReportId, keys, sizeof(keys))) return false;
  keyboardModifier = modifier;
  return true;
}

void beginUsbReconnect(const SharedState& state, const UsbIdentity& identity, uint32_t now) {
  disconnectUsbDevice();
  pendingIdentity = identity;
  identityChanging = true;
  modeDisconnectPending = false;
  identitySwitchAt = now + config::UsbReconnectMs;
  resynchronize(state);
  // A disconnect removes the old keyboard from the host. Clear its local
  // state as well, including when the old endpoint could not accept key-up.
  if (activity.shiftDown()) activity.accepted({ActionKind::ShiftUp, 0, 0}, now);
  keyboardModifier = 0;
}

bool attachSelectedLayout(const UsbIdentity& identity) {
  if (!deviceConfigured || activity.mode() != usbHidMode()) {
    deviceConfigured = configureUsbDevice(hid, activity.mode());
  }
  if (!deviceConfigured) return false;
  setVisibleIdentity(identity);
  neutralKeyboard = keyboardHidEnabled();
  syncMouse = true;
  connectUsbDevice();
  return true;
}

void identityDeviceLoop(const SharedState& state, uint32_t now) {
  if (state.identityRevision != seenIdentityRevision) {
    UsbIdentity identity;
    critical_section_enter_blocking(&sharedLock);
    identity = discoveredIdentity;
    seenIdentityRevision = shared.identityRevision;
    critical_section_exit(&sharedLock);
    // A new revision during a reconnect must replace the queued identity,
    // even if a quickly reinserted donor matches the still-visible old one.
    if (!identityAttached || identityChanging || !identity.same(visibleIdentity())) {
      beginUsbReconnect(state, identity, now);
    }
  }
  if (!identityChanging && identityAttached && activity.mode() != usbHidMode()) {
    if (!modeDisconnectPending) {
      modeDisconnectPending = true;
      modeReleaseAt = now + config::ModeKeyReleaseMs;
      resynchronize(state);
    }
    // outputLoop() gets a bounded opportunity to release an existing Shift
    // on the OLD descriptor. A suspended/broken host cannot stall the switch.
    if (activity.shiftDown() && TinyUSBDevice.mounted() && !TinyUSBDevice.suspended() &&
        static_cast<int32_t>(now - modeReleaseAt) < 0) return;
    beginUsbReconnect(state, visibleIdentity(), now);
  }
  if (identityChanging && static_cast<int32_t>(now - identitySwitchAt) >= 0) {
    if (attachSelectedLayout(pendingIdentity)) {
      identityAttached = true;
      identityChanging = false;
    }
  } else if (!identityAttached && !identityChanging && now - startupAt >= 2000) {
    // With no donor, expose fallback HID (plus CDC only in diagnostics).
    identityAttached = attachSelectedLayout(UsbIdentity{});
  }
}

void outputLoop(const SharedState& s, bool usbActive, uint32_t now) {
  if (!usbActive) {
    forwarder.clear();
    MouseEvent discarded;
    while (queue_try_remove(&mouseQueue, &discarded)) {}
    return;
  }
  Action action = activity.next(now, !forwarder.active() && queue_is_empty(&mouseQueue), s.supported && !s.hostFault);
  if (!hid.ready()) return;
  // Key-up survives mode changes, busy endpoints, receiver removal and suspend.
  if (keyboardHidEnabled() && (neutralKeyboard || action.kind == ActionKind::ShiftUp)) {
    bool wasDown = activity.shiftDown();
    if (sendKeyboard(0)) {
      activity.accepted({ActionKind::ShiftUp, 0, 0}, now);
      if (wasDown) ++shiftReleases;
      neutralKeyboard = false;
    }
    return;
  }
  if (syncMouse) {
    MouseReport report;
    report.buttons = s.buttons;
    if (sendMouse(report)) syncMouse = false;
    return;
  }
  if (!forwarder.active()) {
    MouseEvent event;
    while (queue_try_remove(&mouseQueue, &event)) {
      if (event.epoch == s.epoch) { forwarder.load(event); break; }
    }
  }
  if (forwarder.active()) {
    MouseReport report = forwarder.report();
    if (sendMouse(report)) { forwarder.accepted(report); ++physicalSent; }
    return;
  }
  // Re-evaluate after draining the queue. No physical event is modified by jiggle.
  action = activity.next(now, queue_is_empty(&mouseQueue), s.supported && !s.hostFault);
  if (action.kind == ActionKind::MoveOut || action.kind == ActionKind::MoveBack) {
    MouseReport report;
    report.buttons = s.buttons;
    report.x = action.x;
    report.y = action.y;
    if (sendMouse(report)) {
      activity.accepted(action, now);
      if (action.kind == ActionKind::MoveOut) ++movementsSent;
    }
  } else if (action.kind == ActionKind::ShiftDown && sendKeyboard(KEYBOARD_MODIFIER_LEFTSHIFT)) {
    activity.accepted(action, now);
    ++shiftPresses;
  }
}

void loop() {
  uint32_t now = millis();
  SharedState s = hostSnapshot();
  identityDeviceLoop(s, now);
  bool usbActive = identityAttached && !identityChanging && TinyUSBDevice.mounted() && !TinyUSBDevice.suspended();
  if (s.epoch != seenEpoch || s.overflows != seenOverflows || usbActive != wasUsbActive) {
    resynchronize(s);
    seenEpoch = s.epoch;
    seenOverflows = s.overflows;
  }
  wasUsbActive = usbActive;
  if (s.sequence != seenSequence) {
    activity.physical(s.lastPhysical, s.buttons);
    seenSequence = s.sequence;
  }
  bool fault = s.hostFault || s.overflows || s.decodeErrors;
  if (fault) activity.stop();
  switch (button.update(now, digitalRead(PIN_BUTTON) == LOW)) {
    case ButtonEvent::ShortPress:
      if (usbActive && !modeDisconnectPending && s.supported && !fault) activity.toggle(now);
      else activity.stop();
      break;
    case ButtonEvent::LongPress: activity.changeMode(); break;
    default: break;
  }
  outputLoop(s, usbActive, now);
  // Host claims PIO0 first; NeoPixel then chooses free resources, avoiding a
  // state-machine collision with the timing-critical USB host.
  if (s.ready && !pixelReady) pixelReady = pixel.begin();
  uint32_t color = fault || !s.supported || !usbActive ? pixel.Color(16, 0, 0)
                 : !activity.enabled() ? pixel.Color(0, 0, 4)
                 : activity.mode() == Mode::Movement ? pixel.Color(0, 12, 0) : pixel.Color(14, 6, 0);
  if (pixelReady && color != lastColor) {
    pixel.setPixelColor(0, color);
    pixel.show();
    lastColor = color;
  }
  digitalWrite(LED_BUILTIN, fault ? HIGH : LOW);
#if JIGGY_DIAGNOSTICS
  diagnosticsLoop();
#endif
}

void setup1() {
  while (!__atomic_load_n(&coreReady, __ATOMIC_ACQUIRE)) tight_loop_contents();
  pinMode(PIN_5V_EN, OUTPUT);
  digitalWrite(PIN_5V_EN, HIGH);
  pio_usb_configuration_t pioConfig = PIO_USB_DEFAULT_CONFIG;
  pioConfig.pin_dp = PIN_USB_HOST_DP;
  USBHost.configure_pio_usb(1, &pioConfig);
  tuh_hid_set_default_protocol(HID_PROTOCOL_REPORT);
  bool ok = USBHost.begin(1);
  critical_section_enter_blocking(&sharedLock);
  shared.hostFault = !ok;
  shared.ready = true;
  critical_section_exit(&sharedLock);
}

void loop1() {
  USBHost.task();
  identityHostLoop();
  for (const auto& slot : hostSlots) {
    if (slot.address && slot.supported && tuh_hid_receive_ready(slot.address, slot.instance))
      tuh_hid_receive_report(slot.address, slot.instance);
  }
}

void beginIdentity(uint8_t address) {
  uint32_t generation = identityJob.generation + 1;
  identityJob = IdentityJob{};
  identityJob.generation = generation;
  identityJob.address = address;
  identityJob.stageAt = millis();
  if (!tuh_descriptor_get_device_local(address, &identityJob.device)) {
    identityJob.address = 0;
    return;
  }
  identityJob.identity.vid = identityJob.device.idVendor;
  identityJob.identity.pid = identityJob.device.idProduct;
  identityJob.identity.deviceVersion = identityJob.device.bcdDevice;
  identityJob.identity.valid = identityJob.identity.vid != 0;
  const uint8_t defaultLanguage[] = {4, 3, 9, 4};
  identityJob.identity.languages.assign(defaultLanguage, sizeof(defaultLanguage));
}

void identityStringComplete(tuh_xfer_t* transfer) {
  uint32_t tag = static_cast<uint32_t>(transfer->user_data);
  if ((tag >> 3) != identityJob.generation || (tag & 7) != identityJob.stage || transfer->daddr != identityJob.address) return;
  identityJob.busy = false;
  UsbString value;
  bool ok = transfer->result == XFER_RESULT_SUCCESS &&
    value.assign(reinterpret_cast<const uint8_t*>(identityJob.buffer), transfer->actual_len);
  if (!ok) identityJob.identity.incomplete = true;
  if (identityJob.stage == 0) {
    if (ok && (value.words[0] & 255) >= 4) identityJob.identity.language = value.words[1];
    uint8_t languageBytes[] = {4, 3, static_cast<uint8_t>(identityJob.identity.language), static_cast<uint8_t>(identityJob.identity.language >> 8)};
    identityJob.identity.languages.assign(languageBytes, sizeof(languageBytes));
  } else if (identityJob.stage == 1) identityJob.identity.manufacturer = value;
  else if (identityJob.stage == 2) identityJob.identity.product = value;
  else if (identityJob.stage == 3) identityJob.identity.serial = value;
  ++identityJob.stage;
  identityJob.stageAt = millis();
}

void identityHostLoop() {
  if (!identityJob.address) {
    for (const auto& slot : hostSlots) {
      if (slot.address && slot.supported && tuh_mounted(slot.address)) { beginIdentity(slot.address); break; }
    }
  }
  if (!identityJob.address || identityJob.complete || identityJob.busy) return;
  if (identityJob.stage == 4) {
    critical_section_enter_blocking(&sharedLock);
    discoveredIdentity = identityJob.identity;
    ++shared.identityRevision;
    critical_section_exit(&sharedLock);
    identityJob.complete = true;
    return;
  }
  uint8_t index = identityJob.stage == 0 ? 0 : identityJob.stage == 1 ? identityJob.device.iManufacturer :
                  identityJob.stage == 2 ? identityJob.device.iProduct : identityJob.device.iSerialNumber;
  if (identityJob.stage != 0 && index == 0) { ++identityJob.stage; identityJob.stageAt = millis(); return; }
  uint32_t tag = (identityJob.generation << 3) | identityJob.stage;
  identityJob.busy = tuh_descriptor_get_string(identityJob.address, index, identityJob.stage ? identityJob.identity.language : 0,
    identityJob.buffer, sizeof(identityJob.buffer), identityStringComplete, tag);
  if (!identityJob.busy && millis() - identityJob.stageAt > 1500) {
    identityJob.identity.incomplete = true;
    ++identityJob.stage;
    identityJob.stageAt = millis();
  }
}

extern "C" void tuh_umount_cb(uint8_t address) {
  if (address != identityJob.address) return;
  ++identityJob.generation;
  identityJob.address = 0;
  identityJob.busy = false;
  critical_section_enter_blocking(&sharedLock);
  discoveredIdentity = UsbIdentity{};
  ++shared.identityRevision;
  critical_section_exit(&sharedLock);
}

uint16_t combinedButtons() {
  uint16_t buttons = 0;
  for (const auto& slot : hostSlots) if (slot.address && slot.supported) buttons |= slot.buttons;
  return buttons;
}

extern "C" void tuh_hid_mount_cb(uint8_t address, uint8_t instance, const uint8_t* descriptor, uint16_t length) {
  unsigned index = config::InterfaceCapacity;
  for (unsigned n = 0; n < config::InterfaceCapacity; ++n) if (!hostSlots[n].address) { index = n; break; }
  if (index == config::InterfaceCapacity) {
    critical_section_enter_blocking(&sharedLock);
    shared.hostFault = true;
    critical_section_exit(&sharedLock);
    return;
  }
  HostSlot& slot = hostSlots[index];
  slot.address = address;
  slot.instance = instance;
  slot.buttons = 0;
  slot.supported = slot.parser.parse(descriptor, length) && slot.parser.supported();
  InterfaceInfo info;
  info.address = address;
  info.instance = instance;
  info.protocol = tuh_hid_interface_protocol(address, instance);
  tuh_vid_pid_get(address, &info.vid, &info.pid);
  info.supported = slot.supported;
  info.wheel = slot.supported && slot.parser.hasWheel();
  info.pan = slot.supported && slot.parser.hasPan();
  info.length = descriptor ? min(length, static_cast<uint16_t>(sizeof(info.descriptor))) : 0;
  if (info.length) memcpy(info.descriptor, descriptor, info.length);
  critical_section_enter_blocking(&sharedLock);
  interfaceInfo[index] = info;
  ++shared.connected;
  if (slot.supported) { ++shared.supported; ++shared.epoch; }
  critical_section_exit(&sharedLock);
}

extern "C" void tuh_hid_umount_cb(uint8_t address, uint8_t instance) {
  for (unsigned n = 0; n < config::InterfaceCapacity; ++n) {
    HostSlot& slot = hostSlots[n];
    if (slot.address != address || slot.instance != instance) continue;
    bool supported = slot.supported;
    slot.address = 0;
    slot.buttons = 0;
    critical_section_enter_blocking(&sharedLock);
    interfaceInfo[n] = InterfaceInfo{};
    --shared.connected;
    if (supported) { --shared.supported; ++shared.epoch; }
    shared.buttons = combinedButtons();
    critical_section_exit(&sharedLock);
    break;
  }
}

extern "C" void tuh_hid_report_received_cb(uint8_t address, uint8_t instance, const uint8_t* bytes, uint16_t length) {
  HostSlot* slot = nullptr;
  for (auto& item : hostSlots) if (item.address == address && item.instance == instance) slot = &item;
  if (!slot || !slot->supported) return;
  MouseInput input;
  DecodeResult result = slot->parser.decode(bytes, length, input);
  uint32_t now = millis();
#if JIGGY_DIAGNOSTICS
  if (__atomic_load_n(&rawEnabled, __ATOMIC_ACQUIRE)) {
    RawPacket packet;
    packet.address = address;
    packet.instance = instance;
    packet.length = static_cast<uint8_t>(min(length, static_cast<uint16_t>(sizeof(packet.bytes))));
    memcpy(packet.bytes, bytes, packet.length);
    if (!queue_try_add(&rawQueue, &packet)) {
      critical_section_enter_blocking(&sharedLock);
      ++shared.rawDrops;
      critical_section_exit(&sharedLock);
    }
  }
#endif
  critical_section_enter_blocking(&sharedLock);
  ++shared.reports;
  if (result == DecodeResult::Malformed) ++shared.decodeErrors;
  critical_section_exit(&sharedLock);
  if (result == DecodeResult::Mouse) {
    slot->buttons = (slot->buttons & ~input.buttonMask) | input.buttons;
    uint16_t buttons = combinedButtons();
    critical_section_enter_blocking(&sharedLock);
    bool changed = buttons != shared.buttons || input.x || input.y || input.wheel || input.pan;
    shared.buttons = buttons;
    MouseEvent event;
    event.epoch = shared.epoch;
    event.buttons = buttons;
    event.x = input.x;
    event.y = input.y;
    event.wheel = input.wheel;
    event.pan = input.pan;
    if (changed) { ++shared.events; ++shared.sequence; shared.lastPhysical = now; }
    critical_section_exit(&sharedLock);
    if (changed && !queue_try_add(&mouseQueue, &event)) {
      critical_section_enter_blocking(&sharedLock);
      ++shared.overflows;
      critical_section_exit(&sharedLock);
    }
  }
  // Completion callbacks do not wait for the output endpoint. loop1 retries a
  // receive request if the host endpoint was temporarily busy.
  if (tuh_hid_receive_ready(address, instance)) tuh_hid_receive_report(address, instance);
}

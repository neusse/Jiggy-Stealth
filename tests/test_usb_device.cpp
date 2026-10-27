#include "UsbDevice.h"
#include "UsbIdentity.h"
#include "Config.h"
#include "HidDescriptors.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace jiggy_stealth;
UsbBoundary usbBoundary;
Adafruit_USBD_Device TinyUSBDevice;
Adafruit_USBD_CDC Serial;

extern "C" {
void __wrap_TinyUSB_Device_Init(uint8_t);
void __wrap_TinyUSB_Device_Task(void);
const uint8_t* __wrap_tud_descriptor_device_cb(void);
const uint16_t* __wrap_tud_descriptor_string_cb(uint8_t, uint16_t);
const uint8_t* __real_tud_descriptor_device_cb(void) {
  return reinterpret_cast<const uint8_t*>(&usbBoundary.descriptor);
}
const uint16_t* __real_tud_descriptor_string_cb(uint8_t, uint16_t) { return nullptr; }
void __real_TinyUSB_Device_Task(void) { ++usbBoundary.tasks; }
}

void TinyUSB_Port_InitDevice(uint8_t port) {
  if (port != 0) std::abort();
  ++usbBoundary.starts;
  usbBoundary.firstInterfaces = usbBoundary.interfaces;
  usbBoundary.firstReport.assign(usbBoundary.report, usbBoundary.report + usbBoundary.reportLength);
  usbBoundary.firstDescriptor = *reinterpret_cast<const tusb_desc_device_t*>(__wrap_tud_descriptor_device_cb());
  usbBoundary.initialized = usbBoundary.attached = true;
}

struct ReportShape {
  bool valid = false;
  std::vector<uint32_t> applications, reportIds;
  unsigned outputs = 0;
};

// Independently inspect the actual HID short items. Checking report lengths or
// scanning for one byte sequence alone would miss an unwanted collection.
ReportShape inspectReport(const uint8_t* bytes, size_t length) {
  ReportShape shape;
  uint32_t page = 0, usage = 0;
  unsigned depth = 0;
  for (size_t offset = 0; offset < length;) {
    uint8_t prefix = bytes[offset++];
    if (prefix == 0xfe) return shape;
    unsigned size = (prefix & 3) == 3 ? 4 : (prefix & 3);
    if (size > length - offset) return shape;
    uint32_t value = 0;
    for (unsigned n = 0; n < size; ++n) value |= uint32_t(bytes[offset++]) << (8 * n);
    unsigned type = (prefix >> 2) & 3, tag = prefix >> 4;
    if (type == 1 && tag == 0) page = value;
    if (type == 1 && tag == 8) shape.reportIds.push_back(value);
    if (type == 2 && tag == 0) usage = value;
    if (type == 0) {
      if (tag == 10) {
        if (!depth && value == 1) shape.applications.push_back((page << 16) | usage);
        ++depth;
      } else if (tag == 12) {
        if (!depth) return shape;
        --depth;
      } else if (tag == 9) ++shape.outputs;
      usage = 0;
    }
  }
  shape.valid = depth == 0;
  return shape;
}

unsigned usbDeviceBehavior() {
  unsigned checks = 0;
#define USB_CHECK(condition) do { ++checks; if (!(condition)) { std::fprintf(stderr, "USB FAIL line %d: %s\n", __LINE__, #condition); std::exit(1); } } while (0)
  const auto mouse = inspectReport(MouseHidDescriptor, sizeof(MouseHidDescriptor));
  USB_CHECK(mouse.valid);
  USB_CHECK(mouse.applications == std::vector<uint32_t>{0x10002});
  USB_CHECK(mouse.reportIds == std::vector<uint32_t>{config::MouseReportId});
  USB_CHECK(mouse.outputs == 0);
  const auto combined = inspectReport(HidDescriptor.data(), HidDescriptor.size());
  USB_CHECK(combined.valid);
  USB_CHECK((combined.applications == std::vector<uint32_t>{0x10002, 0x10006}));
  USB_CHECK((combined.reportIds == std::vector<uint32_t>{config::MouseReportId, config::KeyboardReportId}));
  USB_CHECK(combined.outputs == 2); // Keyboard LED bits and output padding.
  usbBoundary = {};
  setVisibleIdentity(UsbIdentity{});
  __wrap_TinyUSB_Device_Init(0);
  __wrap_TinyUSB_Device_Task();
  disconnectUsbDevice();
  USB_CHECK(!usbBoundary.initialized && !usbBoundary.attached);
  USB_CHECK(usbBoundary.starts == 0 && usbBoundary.tasks == 0);
  USB_CHECK(usbBoundary.interfaces.empty() && usbBoundary.serialBegins == 0);
  USB_CHECK(usbBoundary.detaches == 0);

  Adafruit_USBD_HID hid;
  USB_CHECK(configureUsbDevice(hid));
  USB_CHECK(!usbBoundary.initialized && !usbBoundary.attached);
  USB_CHECK(usbBoundary.power == 250);
  USB_CHECK(std::strcmp(usbBoundary.product, config::ProjectName) == 0);
  const std::vector<uint8_t> expected = config::DiagnosticSerial ? std::vector<uint8_t>{3,2,10} : std::vector<uint8_t>{3};
  USB_CHECK(usbBoundary.interfaces == expected);
  USB_CHECK(usbBoundary.serialBegins == (config::DiagnosticSerial ? 1u : 0u));
  USB_CHECK(usbHidMode() == Mode::Movement && !keyboardHidEnabled());
  USB_CHECK(usbReportAllowed(config::MouseReportId));
  USB_CHECK(!usbReportAllowed(config::KeyboardReportId) && !usbReportAllowed(99));
  USB_CHECK(usbBoundary.reportLength == sizeof(MouseHidDescriptor));

  UsbIdentity donor;
  donor.valid = true;
  donor.vid = 0x046d;
  donor.pid = 0xc52b;
  donor.deviceVersion = 0x1211;
  const uint8_t product[] = {4,3,'M',0};
  USB_CHECK(donor.product.assign(product, sizeof(product)));
  setVisibleIdentity(donor);
  connectUsbDevice();
  USB_CHECK(usbBoundary.starts == 1 && usbBoundary.attached);
  USB_CHECK(usbBoundary.firstInterfaces == expected);
  USB_CHECK(usbBoundary.firstReport == std::vector<uint8_t>(std::begin(MouseHidDescriptor), std::end(MouseHidDescriptor)));
  const auto& first = usbBoundary.firstDescriptor;
  USB_CHECK(first.bDeviceClass == (config::DiagnosticSerial ? 0xef : 0));
  USB_CHECK(first.bDeviceSubClass == (config::DiagnosticSerial ? 2 : 0));
  USB_CHECK(first.bDeviceProtocol == (config::DiagnosticSerial ? 1 : 0));
  USB_CHECK(first.idVendor == donor.vid && first.idProduct == donor.pid);
  USB_CHECK(first.bcdDevice == donor.deviceVersion);
  USB_CHECK(first.iProduct == 2 && first.iManufacturer == 0 && first.iSerialNumber == 0);
  const auto* name = __wrap_tud_descriptor_string_cb(2, 0x0409);
  USB_CHECK(name && std::memcmp(name, product, sizeof(product)) == 0);
  USB_CHECK(__wrap_tud_descriptor_string_cb(3, 0x0409) == nullptr);
  __wrap_TinyUSB_Device_Task();
  USB_CHECK(usbBoundary.tasks == 1);

  disconnectUsbDevice();
  USB_CHECK(!usbBoundary.attached && usbBoundary.detaches == 1);
  donor.pid = 0xabcd;
  setVisibleIdentity(donor);
  connectUsbDevice();
  USB_CHECK(usbBoundary.starts == 1 && usbBoundary.attached);
  USB_CHECK(usbBoundary.interfaces == expected);
  USB_CHECK(reinterpret_cast<const tusb_desc_device_t*>(__wrap_tud_descriptor_device_cb())->idProduct == 0xabcd);

  disconnectUsbDevice();
  setVisibleIdentity(UsbIdentity{});
  connectUsbDevice();
  const auto* fallback = reinterpret_cast<const tusb_desc_device_t*>(__wrap_tud_descriptor_device_cb());
  USB_CHECK(fallback->idVendor == 0x239a && fallback->idProduct == 0xcafe);
  USB_CHECK(fallback->bDeviceClass == (config::DiagnosticSerial ? 0xef : 0));
  USB_CHECK(usbBoundary.interfaces == expected && usbBoundary.starts == 1);
  USB_CHECK(usbBoundary.serialBegins == (config::DiagnosticSerial ? 1u : 0u));
  USB_CHECK(!configureUsbDevice(hid, Mode::Shift)); // Cannot modify descriptors while connected.
  USB_CHECK(!keyboardHidEnabled() && usbBoundary.reportLength == sizeof(MouseHidDescriptor));
  for (unsigned cycle = 0; cycle < 20; ++cycle) {
    const Mode mode = cycle % 2 ? Mode::Movement : Mode::Shift;
    disconnectUsbDevice();
    USB_CHECK(configureUsbDevice(hid, mode));
    USB_CHECK(usbBoundary.interfaces == expected); // No duplicate HID or CDC registration.
    USB_CHECK(keyboardHidEnabled() == (mode == Mode::Shift));
    USB_CHECK(usbReportAllowed(config::KeyboardReportId) == (mode == Mode::Shift));
    const std::vector<uint8_t> reports = mode == Mode::Shift
      ? std::vector<uint8_t>(HidDescriptor.begin(), HidDescriptor.end())
      : std::vector<uint8_t>(std::begin(MouseHidDescriptor), std::end(MouseHidDescriptor));
    USB_CHECK(usbBoundary.reportLength == reports.size());
    connectUsbDevice();
    USB_CHECK(usbBoundary.lastReport == reports);
    USB_CHECK(usbBoundary.starts == 1);
  }
  std::printf("PASS USB %s: mouse-only startup, Shift descriptor switching, identity and reconnect/fallback\n", config::BuildProfile);
  return checks;
#undef USB_CHECK
}

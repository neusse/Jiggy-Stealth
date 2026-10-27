#pragma once
// Native boundary fake: tests exercise the actual startup/identity modules and
// observe which interfaces exist when the controller first connects.
#include <stdint.h>
#include <vector>

constexpr uint8_t TUSB_CLASS_MISC = 0xef;
constexpr uint8_t MISC_SUBCLASS_COMMON = 2;
constexpr uint8_t MISC_PROTOCOL_IAD = 1;
struct tusb_desc_device_t {
  uint8_t bLength = 18, bDescriptorType = 1;
  uint16_t bcdUSB = 0x0200;
  uint8_t bDeviceClass = 0, bDeviceSubClass = 0, bDeviceProtocol = 0, bMaxPacketSize0 = 64;
  uint16_t idVendor = 0x239a, idProduct = 0xcafe, bcdDevice = 0x0100;
  uint8_t iManufacturer = 1, iProduct = 2, iSerialNumber = 3, bNumConfigurations = 1;
};
struct UsbBoundary {
  bool initialized = false, attached = false;
  unsigned starts = 0, tasks = 0, serialBegins = 0, attaches = 0, detaches = 0;
  uint16_t power = 0;
  const uint8_t* report = nullptr;
  uint16_t reportLength = 0;
  const char* product = nullptr;
  tusb_desc_device_t descriptor, firstDescriptor;
  std::vector<uint8_t> interfaces, firstInterfaces, firstReport, lastReport;
};
extern UsbBoundary usbBoundary;
class Adafruit_USBD_HID;
class Adafruit_USBD_Device {
 public:
  bool isInitialized() const { return usbBoundary.initialized; }
  void clearConfiguration() {
    usbBoundary.interfaces.clear(); usbBoundary.descriptor = {};
    usbBoundary.report = nullptr; usbBoundary.reportLength = 0;
  }
  bool addInterface(Adafruit_USBD_HID& hid);
  void setProductDescriptor(const char* name) { usbBoundary.product = name; }
  void setConfigurationMaxPower(uint16_t power) { usbBoundary.power = power; }
  void attach() {
    usbBoundary.attached = true; ++usbBoundary.attaches;
    usbBoundary.lastReport.assign(usbBoundary.report, usbBoundary.report + usbBoundary.reportLength);
  }
  void detach() { usbBoundary.attached = false; ++usbBoundary.detaches; }
};
extern Adafruit_USBD_Device TinyUSBDevice;
class Adafruit_USBD_HID {
 public:
  void setReportDescriptor(const uint8_t* bytes, uint16_t length) { report = bytes; reportLength = length; }
  bool begin() {
    if (registered) return true; // Mirrors TinyUSB: begin() cannot rebuild an existing interface.
    registered = true;
    usbBoundary.interfaces.push_back(3);
    usbBoundary.report = report; usbBoundary.reportLength = reportLength;
    return true;
  }
  const uint8_t* report = nullptr;
  uint16_t reportLength = 0;
  bool registered = false;
};
inline bool Adafruit_USBD_Device::addInterface(Adafruit_USBD_HID& hid) {
  usbBoundary.interfaces.push_back(3);
  usbBoundary.report = hid.report; usbBoundary.reportLength = hid.reportLength;
  return true;
}
class Adafruit_USBD_CDC {
 public:
  void begin(uint32_t) {
    ++usbBoundary.serialBegins;
    usbBoundary.interfaces.push_back(2);
    usbBoundary.interfaces.push_back(10);
  }
  void end() { TinyUSBDevice.clearConfiguration(); }
};
extern Adafruit_USBD_Device TinyUSBDevice;
extern Adafruit_USBD_CDC Serial;
void TinyUSB_Port_InitDevice(uint8_t port);

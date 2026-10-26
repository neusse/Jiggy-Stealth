#include "UsbDevice.h"
#include "UsbIdentity.h"
#include "Config.h"
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
  usbBoundary.firstDescriptor = *reinterpret_cast<const tusb_desc_device_t*>(__wrap_tud_descriptor_device_cb());
  usbBoundary.initialized = usbBoundary.attached = true;
}

unsigned usbDeviceBehavior() {
  unsigned checks = 0;
#define USB_CHECK(condition) do { ++checks; if (!(condition)) { std::fprintf(stderr, "USB FAIL line %d: %s\n", __LINE__, #condition); std::exit(1); } } while (0)
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
  configureUsbDevice(hid);
  USB_CHECK(!usbBoundary.initialized && !usbBoundary.attached);
  USB_CHECK(usbBoundary.power == 250);
  USB_CHECK(std::strcmp(usbBoundary.product, config::ProjectName) == 0);
  const std::vector<uint8_t> expected = config::DiagnosticSerial ? std::vector<uint8_t>{3,2,10} : std::vector<uint8_t>{3};
  USB_CHECK(usbBoundary.interfaces == expected);
  USB_CHECK(usbBoundary.serialBegins == (config::DiagnosticSerial ? 1u : 0u));

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
  std::printf("PASS USB %s: startup deferred, correct first-connect classes, identity and reconnect/fallback\n", config::BuildProfile);
  return checks;
#undef USB_CHECK
}

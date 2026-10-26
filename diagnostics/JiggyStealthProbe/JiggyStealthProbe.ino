// Receiver-only diagnostic. This sketch does not generate mouse or keyboard input.
#include <pio_usb.h>
#include <Adafruit_TinyUSB.h>
#include <pico/util/queue.h>

Adafruit_USBH_Host USBHost;
struct Capture {
  uint8_t address, instance, protocol;
  uint16_t vid, pid, length;
  uint8_t descriptor[256];
};
Capture interfaces[8] = {};
queue_t captures;
volatile bool coreReady = false;

void setup() {
  Serial.begin(115200);
  queue_init(&captures, sizeof(Capture), 16);
  __atomic_store_n(&coreReady, true, __ATOMIC_RELEASE);
}

void printCapture(const Capture& c) {
  Serial.printf("HID addr=%u instance=%u protocol=%u vid=%04x pid=%04x length=%u\n", c.address, c.instance, c.protocol, c.vid, c.pid, c.length);
  for (unsigned n = 0; n < c.length; ++n) {
    Serial.printf("%02x%s", c.descriptor[n], (n + 1) % 32 ? " " : "\n");
  }
  Serial.println();
}

void loop() {
  Capture c;
  while (queue_try_remove(&captures, &c)) {
    if (c.instance < 8) interfaces[c.instance] = c;
    if (Serial) printCapture(c);
  }
  while (Serial.available()) {
    char command = Serial.read();
    if (command == 'd') {
      Serial.println("Jiggy-Stealth receiver probe clock=120000000 report_protocol=1");
      for (const auto& item : interfaces) if (item.address) printCapture(item);
    }
  }
}

void setup1() {
  while (!__atomic_load_n(&coreReady, __ATOMIC_ACQUIRE)) tight_loop_contents();
  pinMode(PIN_5V_EN, OUTPUT);
  digitalWrite(PIN_5V_EN, HIGH);
  pio_usb_configuration_t config = PIO_USB_DEFAULT_CONFIG;
  config.pin_dp = PIN_USB_HOST_DP;
  USBHost.configure_pio_usb(1, &config);
  tuh_hid_set_default_protocol(HID_PROTOCOL_REPORT);
  USBHost.begin(1);
}

void loop1() { USBHost.task(); }

extern "C" void tuh_hid_mount_cb(uint8_t address, uint8_t instance, const uint8_t* descriptor, uint16_t length) {
  Capture capture = {};
  capture.address = address;
  capture.instance = instance;
  capture.protocol = tuh_hid_interface_protocol(address, instance);
  tuh_vid_pid_get(address, &capture.vid, &capture.pid);
  capture.length = descriptor ? min(length, static_cast<uint16_t>(sizeof(capture.descriptor))) : 0;
  if (capture.length) memcpy(capture.descriptor, descriptor, capture.length);
  queue_try_add(&captures, &capture);
}

extern "C" void tuh_hid_umount_cb(uint8_t, uint8_t) {}
extern "C" void tuh_hid_report_received_cb(uint8_t, uint8_t, const uint8_t*, uint16_t) {}

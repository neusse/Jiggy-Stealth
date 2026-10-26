# Jiggy-Stealth implementation plan

Researched October 25, 2026. Implementation update: firmware has been written, compiled, and flashed to the reference Feather. The attached 046D:C52B receiver's descriptors were captured, and native parser/control tests passed. The identity-forwarding requirement is implemented: USB IDs and identity strings come from the attached receiver/mouse. Windows has verified the Logitech receiver identity and healthy mouse/keyboard/serial children. The movement distance is now three counts. See [the user guide](docs/USER_GUIDE.md) and [verification](docs/VERIFICATION.md) for current behavior, evidence, and outstanding hardware checks. The sections below retain the original design plan.

Jiggy-Stealth's reference hardware is an Adafruit Feather RP2040 with USB Type A Host, product 5723, and a Logitech M305 with its small USB wireless receiver. The intended result is normal mouse operation through the Feather, with a physical control enabling either occasional small movement or a brief Shift press.

## Feasibility and connections

This board has the required simultaneous USB roles: its native USB-C connection can act as a device to the computer, while its USB-A connector acts as a host through Pico-PIO-USB. Adafruit documents dedicating the second CPU core and a PIO peripheral to hosting. The processor is a dual-core Cortex-M0+ RP2040, nominally 133 MHz, with 264 KB RAM and 8 MB external flash. [Adafruit product](https://www.adafruit.com/product/5723)

```text
Logitech M305 -- existing wireless link --> original USB receiver
                                               |
                                           USB-A port
                                     [Feather RP2040]
                                           USB-C port
                                               |
                                         USB data cable
                                               |
                                            Computer
```

Use the existing, already working receiver. The Feather will process USB input; it does not need to implement Logitech's radio protocol. The computer-facing firmware will expose standard mouse and keyboard HID reports. A keyboard report is necessary to generate Shift, even though the attached physical peripheral is a mouse.

The board can run from USB-C without a battery. Required items are the board, a USB data cable matching the computer's port, and the M305 with its receiver and battery. No additional USB host hardware or soldering is planned. [Adafruit power guide](https://learn.adafruit.com/adafruit-feather-rp2040-with-usb-type-a-host/power-management)

| Board connection | Firmware use |
| --- | --- |
| GPIO16 / `PIN_USB_HOST_DP` | USB-A D+ |
| GPIO17 / `PIN_USB_HOST_DM` | USB-A D- |
| GPIO18 / `PIN_5V_EN` | Enable USB-A peripheral power |
| GPIO7 / `PIN_BUTTON` | Built-in Boot button as runtime control |
| GPIO21 / `PIN_NEOPIXEL` | RGB status LED |
| GPIO20 / `NEOPIXEL_POWER` | RGB LED power enable |

These pins and the runtime button function are documented in the [board pinout guide](https://learn.adafruit.com/adafruit-feather-rp2040-with-usb-type-a-host/pinouts). USB-A power uses a boost converter specified for 1 A peak and a 500 mA resettable fuse; the peak converter rating is not a continuous peripheral current budget. [Adafruit product](https://www.adafruit.com/product/5723)

Keep host power enabled in ordinary passthrough and activity modes. Disabling activity must not disconnect the receiver.

## Recommended firmware

Use Arduino C++ with Earle Philhower's RP2040 core, selecting **Adafruit Feather RP2040 USB Host**. [Adafruit board setup](https://learn.adafruit.com/adafruit-feather-rp2040-with-usb-type-a-host/arduino-ide-setup)

Use Adafruit TinyUSB and Pico PIO USB, select **Adafruit TinyUSB** as the USB stack, and start at **120 MHz**. Adafruit specifies 120 or 240 MHz for PIO-USB; the board's nominal clock is not the setting to use for this host firmware. Record the exact working core/library versions and build options so the result is reproducible. [Adafruit host setup](https://learn.adafruit.com/adafruit-feather-rp2040-with-usb-type-a-host/usb-host-device-info)

Use Adafruit's [`hid_mouse_log_filter`](https://github.com/adafruit/Adafruit_TinyUSB_Arduino/blob/master/examples/DualRole/HID/hid_mouse_log_filter/hid_mouse_log_filter.ino) as a mouse host-flow reference. It requires changes for this application: descriptor-aware input decoding, complete event forwarding, output readiness handling, and keyboard HID output. The product-linked [`hid_remapper`](https://github.com/adafruit/Adafruit_TinyUSB_Arduino/blob/master/examples/DualRole/HID/hid_remapper/hid_remapper.ino) is a keyboard example, not a finished mouse passthrough. TinyUSB's [composite HID descriptors](https://github.com/hathach/tinyusb/blob/master/examples/device/hid_composite/src/usb_descriptors.c) provide a reference for separate mouse and keyboard report IDs.

Proposed responsibilities:

- **Core 1:** initialize and continuously service the USB-A host; identify the receiver's HID interfaces; decode supported mouse reports; enqueue events without waiting for the computer-facing endpoint.
- **Core 0:** service USB device output; drain real mouse events; handle button/LED state; schedule optional activity only when physical input has priority.
- **Shared queue:** use synchronization suitable for both cores. Preserve click/release ordering and movement/scroll totals. Skip synthetic actions while busy. Count overflow, report an error, and resynchronize button state if it occurs; an overflow fails normal-operation acceptance rather than being silently accepted.
- **USB output:** expose a standard relative mouse plus a keyboard using separate report IDs. Choose mouse field widths after inspecting the receiver; avoid truncating wider movement values into an assumed 8-bit structure. Split accumulated values across reports when needed.
- **Diagnostics:** allow descriptor/raw-report logging over USB serial during development, but never require a serial monitor to be open for the mouse to function.

## M305 compatibility work

The M305 manual describes left/right buttons, wheel press, vertical scrolling, and side-to-side scrolling. Include all of those in the baseline and acceptance checks. The manual also describes Logitech software for advanced functions, so reproducing standard HID behavior and preserving Logitech-specific configuration are separate requirements. [Official M305 quick-start guide](https://download01.logitech.com/support/12864.1.0.pdf)

First capture the actual receiver's VID/PID, configuration, HID interfaces, report descriptors, report IDs, and raw samples for each control. Logitech's receiver guidance refers to Unifying-ready M305 models; the mouse name alone does not establish the attached dongle's identity. Use the existing paired receiver rather than choosing or pairing another receiver. [Logitech receiver guidance](https://support.logi.com/hc/en-us/articles/360023384373-Purchasing-a-replacement-USB-receiver-for-my-M305-mouse)

Decode reports using their actual field layout: buttons, signed X/Y, wheel, and horizontal pan or the corresponding tilt events. If necessary, select Report protocol rather than relying on the reduced Boot mouse layout. Do not equate TinyUSB's basic report-ID/usage parser with a full arbitrary HID field decoder. A small decoder for the verified receiver is sufficient for the first version; support for every possible mouse is unnecessary. [TinyUSB host implementation](https://github.com/hathach/tinyusb/blob/master/src/class/hid/hid_host.c) and [host HID example](https://github.com/hathach/tinyusb/blob/master/examples/host/cdc_msc_hid/src/hid_app.c)

The PC will enumerate the Feather as a new HID device. Existing Logitech driver configuration and battery-status features may need additional vendor-protocol work. Ordinary controls, including tilt when available, must be demonstrated before calling the passthrough complete.

## Proposed controls and activity

These are starting settings, not fixed user requirements:

| Mode | Behavior | LED |
| --- | --- | --- |
| Off | Forward physical mouse input continuously | Dim blue |
| Movement | Forward input and occasionally add a tiny relative movement | Green |
| Shift | Forward input and occasionally send Shift down, then Shift up | Amber |
| Receiver absent or unsupported | Pause activity and indicate the problem | Red |

At startup, select Movement but leave activity **off**. A short press of the built-in button toggles activity. A long press of about two seconds changes the selected mode between Movement and Shift and leaves activity off until a short press. Debounce the button and distinguish long presses so they do not also trigger the short-press action. No settings need to be written to flash in version one.

Suggested scheduler:

1. On enabling or real mouse activity, schedule the next possible action 30-60 seconds later using a pseudorandom interval.
2. On expiry, act only if the receiver is connected, the computer-facing USB device is active, at least 30 seconds have passed without physical mouse activity, all physical buttons are up, and the physical-event queue is empty.
3. For Movement, send one relative count in a randomly chosen direction, then the opposite count about 100 ms later if no physical input intervenes. Cancel the return movement if the user resumes moving or clicking. Counts are HID movement units, not guaranteed screen pixels; pointer acceleration and screen boundaries can affect the visible result.
4. For Shift, send left Shift down, followed approximately 30 ms after successful transmission by a separate keyboard release report. Preserve and prioritize a pending release even when activity is disabled or the mode changes. Do not wait in a blocking delay to complete either sequence.
5. After a synthetic action, schedule the next random interval. Synthetic events must not reset the timestamp of the last real mouse activity.

Forward mouse input immediately in every mode. Mouse movement, wheel/pan changes, and button transitions reset the idle timer; repeated empty reports do not. Never inject while a mouse button is held, avoiding interference with dragging.

A separately connected keyboard is outside the Feather's view. Mouse inactivity therefore does not establish that the user is not typing; Shift mode can affect concurrent keyboard input. Movement is the recommended default. Timing and distance remain configurable constants initially, with no computer application required for daily operation.

On receiver removal, reset, computer USB disconnection, or suspend, stop synthetic scheduling and clear stale queued motion. Release virtual keys/buttons when the USB link permits, prioritizing neutral reports before new output on resume. Return to activity off after reconnect/resume. Keep forwarded physical input distinct from virtual state so stopping activity cannot accidentally release a physical button that is still held.

## Implementation milestones and acceptance

1. **Capture a direct-connection baseline.** Confirm M305 movement, clicks, wheel press, vertical scroll, and tilt in appropriate applications with the receiver connected directly to the computer.
2. **Build receiver diagnostics.** Enable host power, enumerate the actual dongle, and capture descriptors and control samples. A boot-mouse test may establish basic connectivity, but does not finish wheel/tilt support.
3. **Finish normal forwarding.** Expose mouse and keyboard HID output, implement the verified receiver decoder and queue, and compare every baseline control with activity off. Validate fast movement, repeated clicks, scrolling without movement, held-button dragging, and receiver sleep/wake. Require no queue overflows at the observed receiver rate.
4. **Add physical controls and Movement.** Verify startup-off, button debounce, LED modes, random scheduling, cancellation on real input, and no movement during dragging.
5. **Add Shift.** Verify each successful key-down has a matching key-up, including disabling immediately after key-down, changing mode, USB suspend/resume, and disconnect/reconnect. Keyboard reporting must continue without mouse motion.
6. **Package and validate.** Export a reproducible UF2 and source with pinned build settings, a short flashing/recovery guide, and a record of the tested receiver identity and supported controls. Run a sustained session with mixed physical and synthetic input and check for stuck keys/buttons, missed clicks, scroll loss, disconnects, and interference with ordinary use.

For firmware recovery, Adafruit documents holding Boot while pressing/releasing Reset to enter the `RPI-RP2` bootloader. [Arduino usage and recovery](https://learn.adafruit.com/adafruit-feather-rp2040-with-usb-type-a-host/arduino-usage)

This is a firmware bridge: with activity off it still relies on working firmware to forward the mouse. It generates HID input; any effect on computer idle, sleep, or locking must be checked on the target computer. The first implementation milestone should be correct M305 forwarding, including wheel and tilt, before enabling synthetic input.

Additional source analysis is in [the firmware research notes](docs/FIRMWARE_RESEARCH.md).

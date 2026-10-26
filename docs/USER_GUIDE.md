# Jiggy-Stealth user guide

Firmware for the **Adafruit Feather RP2040 USB Host (product 5723)**. Connect its USB-C port to the computer and the M305's existing receiver to its USB-A port. The board forwards mouse input and can optionally add a small movement or a brief Shift press.

**Every restart selects Movement mode with jiggle OFF.** Normal mouse forwarding runs regardless of the jiggle setting. USB serial is optional for diagnostics; daily use needs no computer application.

## Controls

| Action | Result |
| --- | --- |
| Power on or Reset | Movement selected; jiggle off |
| Short press of Boot | Toggle jiggle on/off |
| Hold Boot for two seconds | Select the other mode (Movement/Shift); jiggle off until a short press |
| Receiver disconnect or computer USB suspend/reconnect | Jiggle off; selected mode retained until reset |

A long press does not also count as a short press. Holding Boot through startup does not enable jiggle when it is released. Reset is the separate button next to the USB-C connector.

| RGB LED | Meaning |
| --- | --- |
| Dim blue | Normal mouse forwarding; jiggle off |
| Green | Movement enabled |
| Amber | Shift enabled |
| Red | Receiver absent/unsupported, computer USB inactive, or an input/queue error |

With activity enabled, the first action occurs 30-60 seconds after the last real mouse input or enable press. Subsequent actions use randomized intervals. Holding a mouse button blocks activity. Moving, clicking, scrolling, or tilting resets the idle timer.

Movement sends three relative counts and their opposite about 100 ms later. The return is cancelled if physical mouse input intervenes. This is deliberately small; `movements` in diagnostic status counts accepted outward reports. Screen pixels depend on computer pointer settings. Version 1.0.1 increased the initial one-count setting after an idle test showed no visible cursor change.

Shift sends left Shift down and releases it approximately 30 ms later. Key release has priority over mouse input and remains pending through a busy endpoint or USB suspend. A separately connected keyboard is not visible to this device, so Shift mode can affect concurrent typing. Movement is the default.

## USB identity and receiver support

The receiver attached during development identifies as **Logitech 046D:C52B**. Its captured mouse descriptor uses report ID 2, sixteen button bits, packed signed 12-bit X/Y, vertical wheel, and horizontal AC Pan. The parser decodes bit offsets and signed fields from the HID descriptor instead of casting packets into a fixed mouse structure. Captures are in [receiver-probe.txt](receiver-probe.txt) and [receiver-descriptors.txt](receiver-descriptors.txt).

With a receiver or supported USB mouse connected, the computer uses that device's **USB vendor ID, product ID, device revision, manufacturer, product, and serial descriptor**. For the tested receiver this is **Logitech 046D:C52B**. A wireless mouse's own radio identity is behind its receiver; the receiver's USB identity is what gets forwarded. A missing donor serial number stays absent, rather than exposing the Feather's generated serial number.

The firmware reads identity strings asynchronously from the donor in its first advertised USB language. It preserves the raw UTF-16 strings, including Unicode, instead of translating or truncating them through TinyUSB's default ASCII setters. Changing/removing the attached device makes the computer re-enumerate the bridge and turns jiggle off. With no donor, a fallback **Jiggy-Stealth** device remains available for programming and diagnostics after a two-second discovery window.

Mouse and keyboard HID collections share the donor's VID/PID. Windows typically names the keyboard child **HID Keyboard Device**, under the receiver's USB parent. The computer-facing device also has diagnostic serial support. Its interface layout is a bridge-defined composite HID plus CDC layout, rather than a byte-for-byte copy of the donor's configuration. HID is placed first to avoid binding the bridge's mouse to Logitech's vendor-interface driver.

The keyboard interface generates only the optional Shift pulse. Keyboard and vendor-specific reports from the attached receiver are not relayed. Logitech battery displays, custom SetPoint mappings, and pairing/configuration tools are outside this forwarding implementation. Copying the identity fields does not implement those protocols.

**Pair the mouse with its receiver while that receiver is connected directly to the computer.** First confirm ordinary mouse operation there, then move the receiver to the Feather. The bridge does not implement the wireless link or Logitech's pairing protocol.

The parser also accepts supported relative mouse report layouts with up to 16 report IDs and 24 mouse fields per report, but other mice are not hardware-qualified. Missing/truncated descriptors and unsupported layouts do not cause a blind boot-mouse fallback. Host report descriptors are limited to 256 bytes by the pinned TinyUSB configuration; receiver packets are limited to 64 bytes.

## Build on Windows

Install PowerShell 7 (`pwsh`) and Arduino IDE 2, or put Arduino CLI on PATH. From the repository root, use PowerShell:

```powershell
pwsh -File .\scripts\setup.ps1
pwsh -File .\scripts\test.ps1
pwsh -File .\scripts\build.ps1
pwsh -File .\scripts\flash.ps1 -Port COM6
```

Replace `COM6` with the board's actual serial port. **Identity forwarding can change the COM port number** when Windows enumerates the Logitech identity; find the current port in Device Manager or with `python -m serial.tools.list_ports -v`. Setup reuses or installs the pinned RP2040 core and downloads checksum-verified libraries into `.deps/libraries`; it does not replace libraries in your Arduino sketchbook. Tests need MSVC C++ Build Tools or `g++` on PATH. They use the captured receiver descriptor with synthesized report payloads, and exercise the actual parser, output format, button handling, scheduler, and identity-string handling.

Versions and the complete board selection are in [toolchain.lock.json](../toolchain.lock.json):

- Earle Philhower RP2040 core 4.5.3
- Adafruit TinyUSB 3.7.3
- Pico PIO USB 0.7.3
- Adafruit NeoPixel 1.15.2
- Adafruit Feather RP2040 USB Host, **120 MHz**, **Adafruit TinyUSB** stack, no flash filesystem

The build outputs `dist/Jiggy-Stealth.uf2` and its SHA-256 checksum, and keeps intermediate files in `build/JiggyStealth`. Build scripts explicitly prioritize the pinned local libraries and map machine-specific source paths out of public binaries. The NeoPixel dependency currently emits an upstream unsigned-comparison warning; the firmware and native tests compile successfully.

Use the provided build script for source compilation: it supplies the two GNU linker descriptor wrappers needed for identity forwarding and defines `USB_IDENTITY_WRAPPING`. A plain Arduino IDE build intentionally stops with an instruction to use that script, avoiding firmware that silently loses identity forwarding. Arduino IDE can still be installed as the provider of Arduino CLI. Do not select **TinyUSB Host (native)**: USB-C must remain the computer-facing device.

## Flashing and recovery

For installation without building, download **Jiggy-Stealth.uf2** from the [latest GitHub release](https://github.com/neusse/Jiggy-Stealth/releases/latest). The release also provides a source/build ZIP, `FLASHING.txt`, and `SHA256SUMS.txt`. The ZIP contains the complete tagged project, including the prebuilt firmware, build scripts, documentation, and licenses. Building from source still requires the prerequisites above and downloads the pinned dependencies.

The upload script requests the serial bootloader and uploads the compiled sketch. Close serial monitors before uploading. If automatic upload fails:

1. Hold **Boot**.
2. Press and release **Reset**, then release Boot when the `RPI-RP2` drive appears.
3. Copy `dist/Jiggy-Stealth.uf2` onto that drive. The board restarts automatically.

The board needs working firmware to forward mouse input. If it stops responding, connect the receiver directly to the computer while recovering the board.

## Diagnostics

Optional Python diagnostics require `pyserial` (`python -m pip install pyserial`). They never enable activity; that requires the physical button.

```powershell
python .\scripts\capture.py --port COM6 --command status
python .\scripts\capture.py --port COM6 --command descriptors
python .\scripts\capture.py --port COM6 --command raw --seconds 10 --out .\build\mouse-reports.txt
```

Raw capture stops logging on exit. In a serial monitor at 115200 baud, send a single character: `s` for JSON status, `d` for descriptors, `r` to start raw mouse reports, `q` to stop, or `?` for help. No newline is required.

Status includes `enabled`, `mode`, `reports`, real `events`, `forwarded` packets, synthetic `movements`, `shift_presses`, `shift_releases`, `shift_down`, `identity_forwarded`, `usb_vid`, `usb_pid`, and `identity_incomplete`. `reports=0` after moving the mouse means the receiver has not delivered input; first check the mouse's power/battery and direct receiver pairing. A recognized receiver descriptor establishes USB connectivity but does not establish a working wireless pairing. An incomplete identity means at least one advertised string could not be read; inspect the USB connection and donor support before assuming its full name was copied.

`overflows`, `decode_errors`, and `host_fault` should remain zero/false. Input errors stop jiggle; an overflow clears stale queued movement and resynchronizes physical button state. Restart after resolving an error. Raw diagnostic drops are counted separately and do not block mouse forwarding. Serial writes are bounded and run on the device core, never inside the receiver callback.

For receiver-only investigation, `pwsh -File .\scripts\build.ps1 -Probe` and `pwsh -File .\scripts\flash.ps1 -Probe -Port COM6` load a probe that only prints descriptors; it does not forward or generate input. Flash the normal firmware afterward.

## Configuration and verification

Timing constants and `MovementCounts` are in [Config.h](../firmware/JiggyStealth/Config.h). Activity selection and enable state live in RAM, so flashing or resetting cannot restore an enabled setting. PIO-USB hosting runs on core 1 with its own stack; output, scheduling, and diagnostics run on core 0. The host claims PIO resources before the NeoPixel initializes.

Hardware validation evidence and remaining checks are recorded in [verification results](VERIFICATION.md). The researched design is in [IMPLEMENTATION_PLAN.md](../IMPLEMENTATION_PLAN.md).

Dependencies retain their upstream licenses; see [third-party notices](../THIRD_PARTY_NOTICES.md): [Arduino-Pico](https://github.com/earlephilhower/arduino-pico), [Adafruit TinyUSB](https://github.com/adafruit/Adafruit_TinyUSB_Arduino), [Pico-PIO-USB](https://github.com/sekigon-gonnoc/Pico-PIO-USB), and [Adafruit NeoPixel](https://github.com/adafruit/Adafruit_NeoPixel). Setup fetches the pinned source archives, including their notices and licenses.

## Packaging a release

From a clean Git checkout, build the firmware and commit the source, matching UF2/checksum, and documentation. Package an exact committed ref with:

```powershell
pwsh -File .\scripts\package.ps1 -Ref HEAD
```

The script reads the version from that commit and writes release assets to `build/releases/v<version>/`. It creates a source ZIP with one project root folder, extracts the exact committed UF2, adds a quick flashing guide and checksums, and checks required source/license files. It excludes Git history, dependency caches, local captures, and build intermediates. Use the same commit for the Git tag and GitHub release.

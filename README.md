<p align="center">
  <img src="docs/assets/jiggy-stealth-banner.svg" alt="Jiggy-Stealth — USB mouse passthrough with optional idle activity" width="100%">
</p>

<h1 align="center">Jiggy-Stealth</h1>

<p align="center">Your mouse, passed through. A little extra motion when you choose.</p>

<p align="center">
  <a href="docs/USER_GUIDE.md"><img alt="Hardware: RP2040 USB Host" src="https://img.shields.io/badge/RP2040-USB_Host-36cbb8?style=flat-square&amp;labelColor=122630"></a>
  <a href="firmware/JiggyStealth/Config.h"><img alt="Firmware 1.2.0" src="https://img.shields.io/badge/Firmware-1.2.0-8fb9ee?style=flat-square&amp;labelColor=122630"></a>
  <a href="docs/VERIFICATION.md"><img alt="26,471 native checks passed" src="https://img.shields.io/badge/Checks-26%2C471_passed-a8db78?style=flat-square&amp;labelColor=122630"></a>
  <img alt="Jiggle off at startup" src="https://img.shields.io/badge/Startup-Jiggle_OFF-36cbb8?style=flat-square&amp;labelColor=122630">
  <a href="LICENSE"><img alt="License: PolyForm Noncommercial 1.0.0" src="https://img.shields.io/badge/License-PolyForm_NC_1.0.0-e6b868?style=flat-square&amp;labelColor=122630"></a>
</p>

Jiggy-Stealth turns an [Adafruit Feather RP2040 USB Host](https://www.adafruit.com/product/5723) into a USB mouse bridge with optional idle movement or brief Shift presses. It forwards the attached device's USB identity and gives real mouse input priority. Tested with a Logitech M305 and its USB receiver.

**Movement is the default mode. Jiggle starts OFF after every restart.**

**[Download the latest release](https://github.com/neusse/Jiggy-Stealth/releases/latest)** — [ready-to-flash UF2](https://github.com/neusse/Jiggy-Stealth/releases/latest/download/Jiggy-Stealth.uf2), complete source/build ZIP, quick flashing guide, and SHA-256 checksums. Installing the UF2 needs no compiler or development tools.

## Get started

1. Connect the Feather's USB-C port to your computer and plug your paired mouse receiver into its USB-A port.
2. Download [Jiggy-Stealth.uf2](https://github.com/neusse/Jiggy-Stealth/releases/latest/download/Jiggy-Stealth.uf2) and follow the [flashing guide](docs/USER_GUIDE.md#flashing-and-recovery).
3. Short-press **Boot** to enable or disable jiggle. Green means Movement is enabled; dim blue means off.

After 30–60 seconds without mouse activity, Movement adds a tiny excursion and return. Hold Boot for two seconds to select Movement or Shift; the selected mode stays off until a short press. Pair wireless receivers directly on the computer before using the bridge.

USB identity forwarding copies IDs and name strings. The bridge exposes its own composite HID/diagnostic layout; see [USB identity and compatibility](docs/USER_GUIDE.md#usb-identity-and-receiver-support) for the supported behavior.

## Documentation

Start with the **[documentation index](docs/README.md)** for controls, LEDs, setup, USB identity, diagnostics, configuration, and verification.

- [User guide](docs/USER_GUIDE.md) — use, build, flash, and troubleshoot.
- [Verification](docs/VERIFICATION.md) — automated coverage and observed hardware results.
- [Implementation plan](IMPLEMENTATION_PLAN.md) · [Firmware research](docs/FIRMWARE_RESEARCH.md).

To build on Windows with PowerShell 7 and Arduino CLI or Arduino IDE 2 installed:

```powershell
pwsh -File .\scripts\setup.ps1
pwsh -File .\scripts\test.ps1
pwsh -File .\scripts\build.ps1
```

## License

Personal and noncommercial use, modification, and sharing are permitted under the [PolyForm Noncommercial License 1.0.0](LICENSE). Commercial use, including commercial derivatives, requires a separate written commercial license from the repository owner, subject to the full license's permissions. See [licensing details](LICENSING.md) and [NOTICE](NOTICE).

Dependency sources and their independent licenses are listed in [third-party notices](THIRD_PARTY_NOTICES.md).

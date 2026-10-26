# Jiggy-Stealth third-party notices

Jiggy-Stealth uses the components below. Their original licenses and copyright notices continue to apply to those components. The complete pinned library source archives are listed in [toolchain.lock.json](toolchain.lock.json) and fetched by [setup.ps1](scripts/setup.ps1).

The [PolyForm Noncommercial project license](LICENSE) applies to original Jiggy-Stealth work. Third-party components are governed independently by the licenses below; the project's commercial-use restrictions do not replace them. [LICENSING.md](LICENSING.md#scope-and-third-party-rights) preserves the applicable LGPL modification, relinking, and debugging rights for combined firmware.

| Component | Version | License text | Source |
| --- | --- | --- | --- |
| Arduino-Pico RP2040 core | 4.5.3 | [LGPL 2.1](docs/licenses/arduino-pico.txt) | [earlephilhower/arduino-pico](https://github.com/earlephilhower/arduino-pico/tree/4.5.3) |
| Adafruit TinyUSB Library | 3.7.3 | [MIT](docs/licenses/adafruit-tinyusb.txt) | [adafruit/Adafruit_TinyUSB_Arduino](https://github.com/adafruit/Adafruit_TinyUSB_Arduino) |
| Pico PIO USB | 0.7.3 | [MIT](docs/licenses/pico-pio-usb.txt) | [sekigon-gonnoc/Pico-PIO-USB](https://github.com/sekigon-gonnoc/Pico-PIO-USB) |
| Adafruit NeoPixel | 1.15.2 | [LGPL 3.0](docs/licenses/adafruit-neopixel.txt) | [adafruit/Adafruit_NeoPixel](https://github.com/adafruit/Adafruit_NeoPixel) |
| Raspberry Pi Pico SDK, bundled by the pinned core | Core-bundled revision | [BSD 3-clause](docs/licenses/pico-sdk.txt) | [raspberrypi/pico-sdk](https://github.com/raspberrypi/pico-sdk) |

The Arduino core and SDK contain additional third-party components with notices in their source trees. Rebuilding the supplied UF2 uses the source, dependency versions, and linker options in this repository; the dependency archives retain their complete notices. The SVG hero is a local Jiggy-Stealth asset.

# Jiggy-Stealth documentation

Jiggy-Stealth bridges a USB mouse or wireless receiver through the Adafruit Feather RP2040 USB Host, with optional idle activity controlled by the board's Boot button.

| Document | What it covers |
| --- | --- |
| [User guide](USER_GUIDE.md) | Connections, controls, LEDs, USB identity, building, flashing, diagnostics, and configuration |
| [Verification](VERIFICATION.md) | Native checks, hardware results, tested receiver, and remaining coverage |
| [Implementation plan](../IMPLEMENTATION_PLAN.md) | Hardware architecture and the original implementation stages |
| [Firmware research](FIRMWARE_RESEARCH.md) | Primary sources, USB host/device constraints, and receiver report formats |
| [Third-party notices](../THIRD_PARTY_NOTICES.md) | Pinned dependency sources and their license texts |
| [Licensing](../LICENSING.md) | Personal/noncommercial permissions, commercial license inquiries, and third-party rights |

## Useful entry points

- **Use the device:** [controls and LED states](USER_GUIDE.md#controls).
- **Install firmware:** [flashing and recovery](USER_GUIDE.md#flashing-and-recovery).
- **Build the project:** [Windows build instructions](USER_GUIDE.md#build-on-windows).
- **Understand identity forwarding:** [USB identity and receiver support](USER_GUIDE.md#usb-identity-and-receiver-support).
- **Inspect behavior:** [diagnostics](USER_GUIDE.md#diagnostics).
- **Change timing or movement distance:** [Config.h](../firmware/JiggyStealth/Config.h).

The tested receiver's static USB descriptors are available in [receiver-probe.txt](receiver-probe.txt) and [receiver-descriptors.txt](receiver-descriptors.txt). Raw local session recordings and build caches are excluded from this repository.

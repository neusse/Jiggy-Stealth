# Jiggy-Stealth verification

Jiggy-Stealth targets the **Adafruit Feather RP2040 USB Host, product 5723**. The reference hardware session used a **Logitech M305** paired with a **046D:C52B** receiver on Windows.

## Automated checks

The native suite exercises the same parser, scheduler, button logic, identity helpers, and report format used by the firmware. It passes **26,471 checks**, covering:

- Packed signed 12-bit receiver movement across the valid X/Y range, sixteen button bits, vertical wheel, and horizontal pan.
- Report-ID filtering, malformed/truncated data, and splitting large output deltas without losing movement or scrolling.
- Button debounce, a single long-press event, startup with Boot held, Movement selected by default, and activity disabled at startup.
- Idle gating, physical-input priority, cancellation of return movement, pending Shift release, and timer wrap.
- Exact raw UTF-16 identity strings, Unicode, absent donor serial numbers, identity comparison, and malformed/max-length USB string descriptors.

The renamed **1.2.0** firmware also compiles successfully for the pinned RP2040 target: 94,508 bytes of flash and 67,084 bytes of static RAM. The UF2 and SHA-256 checksum are in [dist](../dist/).

Run the suite from the repository root:

```powershell
pwsh -File .\scripts\test.ps1
```

## Observed hardware results

The working behavior was verified on firmware **1.1.1**, before the Jiggy-Stealth branding update. The **1.2.0** source uses the new project name, sketch paths, namespace, and fallback USB product string; mouse forwarding and activity behavior are retained. Rebuild and native checks validate the renamed project. The reference board does not need to be reflashed merely to publish this repository.

| Check | Observation |
| --- | --- |
| USB host | Three receiver HID interfaces enumerated; the mouse interface exposed wheel and horizontal pan |
| Forwarding | A session delivered 1,431 physical events and 1,431 forwarded packets with zero decode errors and overflows |
| Host identity | Windows reported `046D:C52B`, device revision `1211`, product **USB Receiver**, and healthy mouse, keyboard, and serial drivers |
| Keyboard child | **HID Keyboard Device**, sharing the receiver's VID/PID |
| Startup | Movement selected, jiggle disabled, no held Shift, and complete identity forwarding |
| Physical control | Short-press Boot enabled Movement |
| Jiggle | Synthetic count increased while the physical input count stayed unchanged; Windows cursor moved two pixels upward and returned 93 ms later |
| Jiggle errors | Zero decode errors and queue overflows during the confirmed pulse |

The native descriptor fixture is in [M305Descriptor.h](../tests/M305Descriptor.h); static USB captures are in [receiver-probe.txt](receiver-probe.txt) and [receiver-descriptors.txt](receiver-descriptors.txt). Raw session logs, screen coordinates, local device topology, and build caches are retained locally rather than published.

## Coverage limits

The automated Shift checks pass, but a physical Shift pulse session has not been recorded. Computer suspend/resume, receiver reconnects, sustained mixed input, and each application-level click/drag/scroll behavior still need wider hardware acceptance. Other mice and receivers are not hardware-qualified by the M305 result.

Identity forwarding copies USB identity fields. Jiggy-Stealth supplies its own composite HID/CDC interface layout and does not relay Logitech vendor configuration or pairing protocols. See the [user guide](USER_GUIDE.md#usb-identity-and-receiver-support).

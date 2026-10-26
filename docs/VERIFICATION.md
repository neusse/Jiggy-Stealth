# Jiggy-Stealth verification

Jiggy-Stealth targets the **Adafruit Feather RP2040 USB Host, product 5723**. The reference hardware session used a **Logitech M305** paired with a **046D:C52B** receiver on Windows.

## Automated checks

The native suite exercises the same parser, scheduler, button logic, identity helpers, and report format used by the firmware. It passes **26,500 checks for each of Production and Diagnostics**, covering:

- Packed signed 12-bit receiver movement across the valid X/Y range, sixteen button bits, vertical wheel, and horizontal pan.
- Report-ID filtering, malformed/truncated data, and splitting large output deltas without losing movement or scrolling.
- Button debounce, a single long-press event, startup with Boot held, Movement selected by default, and activity disabled at startup.
- Idle gating, physical-input priority, cancellation of return movement, pending Shift release, and timer wrap.
- Exact raw UTF-16 identity strings, Unicode, absent donor serial numbers, identity comparison, and malformed/max-length USB string descriptors.
- The actual USB startup and identity modules against a native boundary fake: no controller/CDC startup through the core initializer, guarded early USB tasks, HID-first interface selection, correct classes and donor identity at first connection, one-time controller initialization, and identity reconnect/fallback handling in both profiles.

Both **1.3.0** profiles compile successfully for the pinned RP2040 target:

| Profile | Flash | Static RAM | UF2 |
| --- | --- | --- | --- |
| Production | 92,500 bytes | 58,868 bytes | [Jiggy-Stealth.uf2](../dist/Jiggy-Stealth.uf2) |
| Diagnostics | 94,564 bytes | 67,084 bytes | [Jiggy-Stealth-Diagnostics.uf2](../dist/Jiggy-Stealth-Diagnostics.uf2) |

Matching SHA-256 files are in [dist](../dist/). The native boundary fake checks application USB initialization and descriptor choices, not USB electrical timing or the complete upstream stack. Linked ARM disassembly separately confirms that core startup calls the deferred initializer wrapper in both builds. The Production ELF has no `Adafruit_USBD_CDC::begin` symbol; Diagnostics retains it.

Run the suite from the repository root:

```powershell
pwsh -File .\scripts\test.ps1
```

## Observed hardware results

### Version 1.3.0 profiles

Both images were flashed to the reference board. Diagnostics reported version **1.3.0**, build **diagnostics**, CDC enabled, complete Logitech identity forwarding, **Movement selected**, **jiggle off**, no held Shift, and zero decode errors, queue overflows, or host faults. Windows then enumerated Production as a healthy **HID-compliant mouse**, **HID Keyboard Device**, and **USB Input Device** using `046D:C52B`, revision `1211`, and the `HidUsb` service. The device had no present serial/COM child; the other computer serial ports were unrelated Bluetooth ports. Production's compatible IDs reported USB class `03` (HID).

The v1.3.0 profile change retains the parser and activity logic from the earlier hardware session below. Physical input and manual recovery observations are recorded separately as they are completed.

### Earlier forwarding and jiggle checks

The working behavior was verified on firmware **1.1.1**, before the Jiggy-Stealth branding update. The **1.2.0** source uses the new project name, sketch paths, namespace, and fallback USB product string; mouse forwarding and activity behavior are retained. Rebuild and native checks validate the renamed project.

On October 25, 2026, the published **1.2.0** UF2 was flashed to the reference board through COM6. The upload image matched the published UF2 and its SHA-256 checksum. Post-flash diagnostics confirmed version 1.2.0, **Movement selected**, **jiggle disabled**, no held Shift, complete Logitech identity forwarding, and zero decoding errors, queue overflows, or host faults. Activity was left off for the next physical Boot press.

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

Identity forwarding copies USB identity fields. Jiggy-Stealth supplies its own mouse/keyboard HID layout, with CDC only in Diagnostics, and does not relay Logitech vendor configuration or pairing protocols. See the [user guide](USER_GUIDE.md#usb-identity-and-receiver-support).

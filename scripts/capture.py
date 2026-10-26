"""Read Jiggy-Stealth-Diagnostics firmware; production has no serial port. This cannot enable activity."""
import argparse
import json
from pathlib import Path
import sys
import time

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    raise SystemExit("Install the optional diagnostic dependency: python -m pip install pyserial")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", help="COM port; otherwise auto-select the default Feather or tested Logitech identity")
    parser.add_argument("--command", choices=["status", "descriptors", "raw"], default="status")
    parser.add_argument("--seconds", type=float, default=3)
    parser.add_argument("--out", type=Path)
    args = parser.parse_args()
    if not 0 < args.seconds <= 60:
        parser.error("--seconds must be greater than zero and at most 60")
    port = args.port
    if not port:
        matches = [p.device for p in list_ports.comports()
                   if (p.vid == 0x239A and p.pid in (0x8129, 0xC129, 0xCAFE))
                   or (p.vid == 0x046D and p.pid == 0xC52B)]
        if len(matches) != 1:
            parser.error("Specify --port COMx; could not identify exactly one Feather USB Host. Production has no COM port; install Jiggy-Stealth-Diagnostics.uf2 for capture.")
        port = matches[0]
    data = bytearray()
    with serial.Serial(port, 115200, timeout=0.1, write_timeout=1) as connection:
        time.sleep(0.2)
        connection.reset_input_buffer()
        connection.write(b"q" + {"status": b"s", "descriptors": b"d", "raw": b"r"}[args.command])
        try:
            end = time.monotonic() + args.seconds
            while time.monotonic() < end:
                data.extend(connection.read(4096))
        finally:
            if args.command == "raw":
                connection.write(b"q")
    output = data.decode("utf-8", errors="replace").replace("\r\n", "\n")
    if args.command == "status":
        statuses = []
        for line in output.splitlines():
            try:
                item = json.loads(line)
                if "version" in item:
                    statuses.append(item)
            except (json.JSONDecodeError, TypeError):
                pass
        if not statuses:
            raise SystemExit(f"No firmware status received from {port}. Is Jiggy-Stealth running?")
        output = json.dumps(statuses[-1], indent=2) + "\n"
    if args.out:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(output, encoding="utf-8")
    sys.stdout.write(output)


if __name__ == "__main__":
    main()

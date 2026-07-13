#!/usr/bin/env python3
"""Interactive USB CDC monitor for ucds-box.

Usage:
    python3 usb_monitor.py [port] [baud]

The port is detected automatically when omitted. Keyboard input is forwarded
immediately, so commands such as ``h`` and ``1`` do not require Enter. Output
is displayed and saved to logs/usb_YYYYMMDD_HHMMSS.log.
"""

import datetime
import glob
import os
import select
import signal
import sys
import termios
import time
import tty

try:
    import serial
except ImportError:
    print("ERROR: pyserial is not installed. Run: pip3 install pyserial")
    sys.exit(1)


DEFAULT_BAUD = 115200
PORT_PATTERNS = ("/dev/cu.usbmodem*", "/dev/ttyACM*")


def find_port():
    for pattern in PORT_PATTERNS:
        ports = sorted(glob.glob(pattern))
        if ports:
            return ports[0]
    return None


def timestamp():
    return datetime.datetime.now().strftime("%H:%M:%S.%f")[:-3]


def main():
    requested_port = sys.argv[1] if len(sys.argv) > 1 else None
    baud = int(sys.argv[2]) if len(sys.argv) > 2 else DEFAULT_BAUD

    os.makedirs("logs", exist_ok=True)
    log_path = datetime.datetime.now().strftime("logs/usb_%Y%m%d_%H%M%S.log")
    log_file = open(log_path, "ab", buffering=0)
    print(f"Log: {log_path}")
    print("Waiting for ucds-box USB CDC port...")

    stdin_fd = sys.stdin.fileno()
    old_tty = termios.tcgetattr(stdin_fd)
    device = None
    line_buffer = b""
    last_probe = 0.0
    closed = False

    def save_output(data):
        nonlocal line_buffer
        line_buffer += data
        while b"\n" in line_buffer:
            line, _, line_buffer = line_buffer.partition(b"\n")
            log_file.write(f"[{timestamp()}] ".encode() + line + b"\n")

    def cleanup(_signal=None, _frame=None):
        nonlocal closed
        if closed:
            return
        closed = True
        termios.tcsetattr(stdin_fd, termios.TCSADRAIN, old_tty)
        sys.stdout.write("\r\nClosing USB monitor...\r\n")
        sys.stdout.flush()
        if device is not None:
            try:
                device.close()
            except Exception:
                pass
        if line_buffer:
            log_file.write(f"[{timestamp()}] ".encode() + line_buffer + b"\n")
        log_file.close()

    signal.signal(signal.SIGINT, cleanup)
    signal.signal(signal.SIGTERM, cleanup)
    tty.setraw(stdin_fd)

    try:
        while not closed:
            if device is None:
                now = time.monotonic()
                if now - last_probe >= 0.2:
                    last_probe = now
                    port = requested_port or find_port()
                    if port:
                        try:
                            device = serial.Serial(port, baud, timeout=0)
                            sys.stdout.write(f"\r\nConnected: {port} @ {baud}\r\n")
                            sys.stdout.write("Commands: h=help, 1=toggle transmission, Ctrl+C=quit\r\n")
                            sys.stdout.flush()
                        except (OSError, serial.SerialException):
                            device = None

            inputs = [sys.stdin] if device is None else [sys.stdin, device.fileno()]
            readable, _, _ = select.select(inputs, [], [], 0.05)

            if device is not None and device.fileno() in readable:
                try:
                    data = device.read(4096)
                    if data:
                        sys.stdout.buffer.write(data)
                        sys.stdout.buffer.flush()
                        save_output(data)
                except (OSError, serial.SerialException):
                    sys.stdout.write("\r\nUSB disconnected; waiting for reconnect...\r\n")
                    sys.stdout.flush()
                    try:
                        device.close()
                    except Exception:
                        pass
                    device = None

            if sys.stdin in readable:
                key = sys.stdin.buffer.read(1)
                if key == b"\x03":
                    break
                if key and device is not None:
                    try:
                        device.write(key)
                    except (OSError, serial.SerialException):
                        pass
    finally:
        cleanup()


if __name__ == "__main__":
    main()

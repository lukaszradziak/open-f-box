# ford-box

This project was developed and tested for a 2017 Ford Mondeo Mk5 using the
CGEA electrical architecture.

- **HS/CAN1 @ 500000 bps**;
- MCU: PB8/PB9, STB=PB3;
- transceiver: U4;
- OBD: pins 3/11.

## hardware

- red UCDS from aliexpress
- st-link v2

## build and upload (ST-Link)

Install PlatformIO, connect ST-Link and flash the firmware:

```bash
pio run -t upload
```

## USB serial monitor

ST-Link is used only for flashing. Runtime output and commands use the board's
USB CDC port (PA11/PA12).

Install the Python dependency once:

```bash
pip3 install -r requirements.txt
```

After flashing, ST-Link may be disconnected. Connect the board's USB port with
a data-capable USB cable and start the monitor:

```bash
python3 usb_monitor.py
```

The monitor automatically detects `/dev/cu.usbmodem*` on macOS (or
`/dev/ttyACM*` on Linux), reconnects after a USB disconnect and saves output to
`logs/usb_<date>_<time>.log`. A port can also be selected explicitly:

```bash
python3 usb_monitor.py /dev/cu.usbmodemXXXXXXXX 115200
```

Press `Ctrl+C` to close the monitor.

Commands are handled immediately, without pressing Enter:

| Command | Action                                       |
|---------|----------------------------------------------|
| `h`     | Show help and the current transmission state |
| `1`     | Toggle periodic `0x2B4` transmission ON/OFF  |

The periodic transmission is enabled by default. Disabling it also pauses the
mock vehicle-state updates. After enabling it again, the next record is sent
after `DATA_INTERVAL_MS` (currently 1000 ms).

## OBD

| Element   | MCU pins           | OBD pins | Speed    |
|-----------|--------------------|----------|----------|
| CAN1 / U4 | PB8/PB9, STB PB3   | 3/11     | 500 kbps |
| CAN2 / U3 | PB12/PB13, STB PB7 | 6/14     | 500 kbps |
| CAN3 / U2 | PB5/PB6, STB PB4   | 1/8      | 125 kbps |

| Pin OBD-II | Signal            | Role on board |
|-----------:|-------------------|---------------|
|          1 | `CAN3H`           | CAN High 3    |
|          3 | `CAN1H`           | CAN High 1    |
|          4 | `GND`             | ground        |
|          5 | `GND`             | ground        |
|          6 | `CAN2H`           | CAN High 2    |
|          8 | `CAN3L`           | CAN Low 3     |
|         11 | `CAN1L`           | CAN Low 1     |
|         14 | `CAN2L`           | CAN Low 2     |
|         16 | `+12V`            | power         |

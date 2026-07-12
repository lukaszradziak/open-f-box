# ford-box

This project was developed and tested for a 2017 Ford Mondeo Mk5 using the
CGEA electrical architecture.

- **HS/CAN1 @ 500000 bps**;
- MCU: PB8/PB9, STB=PB3;
- transceiver: U4;
- OBD: piny 3/11.

## hardware

- red UCDS from aliexpress
- st-link v2

## build and upload (ST-Link)

```bash
pio run -t upload
```

## monitor (RTT)

```bash
python3 rtt_monitor.py
```

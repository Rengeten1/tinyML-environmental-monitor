# Hardware Wiring Layout

## Components

- ESP32-S3 DevKitC-1
- DHT11 sensor module
- Jumper wires
- USB cable

## Pin Mapping

| DHT11 Pin | ESP32-S3 Pin | Purpose |
|---|---|---|
| VCC | 3V3 | Sensor power |
| GND | GND | Common ground |
| DATA | GPIO4 | Temperature/humidity data line |

## Board-Side Notes

- `GPIO4` is configured as `DHTPIN` in both firmware projects.
- Built-in RGB LED pin is `GPIO48` in code (`RGB_BUILTIN` / `RGB_PIN`).
- Keep sensor wires short and stable to reduce noisy readings.

## Wiring Diagram Placeholder

Add your diagram image to this folder and reference it here.

Example:

```md
![ESP32-S3 to DHT11 wiring](./wiring-diagram.png)
```

## Bring-Up Checklist

1. Verify 3V3 and GND are connected correctly.
2. Confirm DHT11 DATA goes to `GPIO4`.
3. Flash `firmware/data-collection` and check serial output at `115200`.
4. If values are `NaN`, recheck wiring and sensor power.
5. Flash `firmware/inference-system` and verify dashboard updates.

## Optional Improvements

- Add a photo of the actual breadboard wiring.
- Include board revision and sensor model details.
- Add troubleshooting notes for unstable humidity/temperature readings.

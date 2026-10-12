# crunchtime-reed-switch

An ESP8266 (D1 Mini) sensor node for live performance voting. Detects magnet proximity via a reed switch and sends OSC messages over WiFi to [crunchtime-tally](../crunchtime-tally) whenever the switch opens or closes. Messages are retried until an `/ack` is received.

## How it works

**Hardware**

- ESP8266 D1 Mini running on WiFi
- Reed switch on pin `D1` (GPIO5), with internal pull-up — `LOW` = closed/magnet present, `HIGH` = open/magnet removed
- 50ms debounce on switch state changes

**State machine**

| State | Condition | Action |
|---|---|---|
| `Idle` | Switch is closed (magnet present) | Wait |
| `Idle → Detected` | Switch opens (magnet removed) | Send `/cue/{sensorId}on/start` |
| `Detected` | Switch is open | Wait |
| `Detected → Idle` | Switch closes (magnet returns) | Send `/cue/{sensorId}off/start` |

**OSC output (UDP)**

Each message is sent to the tally server IP and port 8765, and retried every 250ms until an `/ack` is received back on local port 53001.

| Message | Trigger |
|---|---|
| `/cue/{sensorId}on/start` | Magnet removed (switch opened) |
| `/cue/{sensorId}off/start` | Magnet replaced (switch closed) |

`sensorId` is a unique integer set per device (1–36 for a 12-voter, 3-choice setup).

## Configuration

### 1. Copy `env_config.example.h` to `env_config.h`

`env_config.h` is gitignored — it holds WiFi credentials and server IPs per environment.

```cpp
// env_config.h
#pragma once

#define HOME_SSID "mywifi"
#define HOME_PASSWORD "mypassword"
#define HOME_IP IPAddress(192, 168, 1, 169)

#define CRUNCH_TALLY IPAddress(10, 0, 0, 73)   // IP of crunchtime-tally server
// add more environments as needed
```

### 2. Edit `src/main.cpp` for each sensor

```cpp
const WifiEnv wifi(HOME_SSID, HOME_PASSWORD, CRUNCH_TALLY);  // pick environment

const unsigned int sensorId = 1;     // unique ID for this sensor (1–36)
const char *hostname = "ctsensor-1"; // hostname shown on network
```

Each physical sensor unit needs a unique `sensorId`. The tally server uses this to map sensors to seats and choices — see [crunchtime-tally](../crunchtime-tally) for the mapping.

## Build & flash

Uses [PlatformIO](https://platformio.org/). Target board: `d1_mini`.

```sh
pio run                          # build
pio run --target upload          # build + flash
pio device monitor               # open serial monitor (9600 baud)
```

On boot the device waits a random 0–15 seconds before connecting to WiFi. This staggers connection attempts when multiple units power on simultaneously. Successful startup prints `WiFi Connected!` and `Sensor ID {n}` to serial.

If WiFi drops during operation, the main loop automatically reconnects.

## Known issues & build notes

**Windows 11 — Silicon Labs CP210x driver**  
Do not use the CP210x Windows driver v6.7.6 with the D1 Mini — it won't work. Use v6.7.4 or earlier.

**BluetoothSerial compile error**  
The CMAT OSC library includes Bluetooth headers that aren't available on ESP8266. If you see:
```
SLIPEncodedBluetoothSerial.h:11:10: fatal error: BluetoothSerial.h: No such file or directory
```
Delete these two files from `.pio/libdeps/d1_mini/OSC/`:
- `SLIPEncodedBluetoothSerial.h`
- `SLIPEncodedBluetoothSerial.cpp`

**Lonely Binary D1 Mini boards**  
Replacement boards using the Lonely Binary ESP8266EX variant are supported via the `d1_mini` environment in `platformio.ini`.

**Apple Silicon**  
Rosetta is required for the C++ toolchain. Open Terminal's info panel and tick *Open with Rosetta* — Rosetta will install on first launch.

## License

MIT — see source header for full text.

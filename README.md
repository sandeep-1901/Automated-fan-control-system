# Home Autonomous Fan Control System
### ESP32 + DHT11 + Relay + LCD + Web Dashboard

---

## Overview

A complete stand-alone temperature-controlled fan system built on ESP32. The fan switches automatically when temperature crosses a user-set threshold. The threshold can be changed physically (buttons on the device) or remotely via a built-in web dashboard. A 16×2 LCD displays live readings at all times.

---

## Hardware Components

| Component | Model / Spec | Qty |
|---|---|---|
| Microcontroller | ESP32 Dev Board (38-pin) | 1 |
| Temperature/Humidity Sensor | DHT11 | 1 |
| Relay Module | 5V Single-channel, Active-LOW | 1 |
| LCD Display | 16×2 I2C LCD (address 0x27) | 1 |
| Push Buttons | Tactile momentary buttons | 3 |
| Resistors | 10kΩ pull-up (if no internal pull-up) | 3 |
| Fan | 5V or 12V DC fan (match relay rating) | 1 |
| Power Supply | 5V 2A USB or wall adapter | 1 |
| Breadboard + Jumper Wires | — | as needed |

---

## Wiring Diagram

```
ESP32 GPIO 4  ────────────────── DHT11 DATA (pin 2)
                                  DHT11 VCC  → 3.3V
                                  DHT11 GND  → GND
                                  (10kΩ pull-up between DATA and 3.3V)

ESP32 GPIO 26 ────────────────── RELAY IN
              (relay module)      RELAY VCC  → 5V
                                  RELAY GND  → GND
                                  RELAY NO   → Fan (+) terminal
                                  Common     → Power Supply (+)
                                  Fan (−)    → Power Supply (−)

ESP32 GPIO 21 (SDA) ──────────── LCD SDA
ESP32 GPIO 22 (SCL) ──────────── LCD SCL
                                  LCD VCC    → 5V
                                  LCD GND    → GND

ESP32 GPIO 34 ─── [BTN_UP]  ─── GND   (threshold +1 °C)
ESP32 GPIO 35 ─── [BTN_DN]  ─── GND   (threshold −1 °C)
ESP32 GPIO 32 ─── [BTN_SET] ─── GND   (save to flash)

ESP32 GPIO 2  ────────────────── Status LED (+ 220Ω → GND)
```

> **Note:** GPIOs 34 & 35 on ESP32 are input-only (no internal pull-up).  
> Add a 10kΩ resistor between each button pin and 3.3V externally.

---

## Circuit Schematic (ASCII)

```
           3.3V ──┬── 10kΩ ──┬──── DHT11 DATA → GPIO4
                  │           │
                  └── DHT11 VCC     DHT11 GND → GND

5V ─────── RELAY VCC
GND ─────── RELAY GND
GPIO26 ──── RELAY IN
            RELAY COM ──── PSU (+)
            RELAY NO  ──── FAN (+) ──── FAN (−) → PSU (−)

5V ─────── LCD VCC           SDA ── GPIO21
GND ─────── LCD GND           SCL ── GPIO22

GPIO34 ──── [BTN_UP] ──── 10kΩ ──── 3.3V   (also GND when pressed)
GPIO35 ──── [BTN_DN] ──── 10kΩ ──── 3.3V
GPIO32 ──── [BTN_SET] ─── internal pull-up via INPUT_PULLUP
```

---

## Button Functions

| Button | Single Press | Combined |
|---|---|---|
| **UP** (GPIO 34) | Threshold + 1 °C | UP + DOWN simultaneously → toggle Auto/Manual mode |
| **DOWN** (GPIO 35) | Threshold − 1 °C | (same) |
| **SET** (GPIO 32) | Save current threshold to flash memory | — |

Threshold range: **15 °C – 50 °C**

---

## LCD Display Layout

```
Row 0:  T:27.5°C H:65%
Row 1:  Set:28°C [FAN] ON  A
```

- `A` = Auto mode, `M` = Manual mode
- Fan icon blinks (custom character) when fan is ON

---

## Web Dashboard

Once connected to WiFi, open a browser and navigate to the ESP32's IP address (shown briefly on LCD at boot).

**Endpoints:**

| Endpoint | Method | Description |
|---|---|---|
| `/` | GET | Full web dashboard (HTML) |
| `/api/data` | GET | JSON telemetry (temp, humidity, fan state, threshold) |
| `/api/set` | POST | Update threshold / mode / manual fan state |

**Example API call:**
```bash
# Change threshold to 30°C
curl -X POST http://<ESP32_IP>/api/set \
     -H "Content-Type: application/json" \
     -d '{"threshold": 30}'

# Switch to manual mode and turn fan ON
curl -X POST http://<ESP32_IP>/api/set \
     -d '{"auto_mode": false, "manual_fan": true}'
```

---

## Operating Modes

### Auto Mode (default)
Fan turns ON automatically when `temperature ≥ threshold`.  
Fan turns OFF when `temperature < threshold`.

### Manual Mode
Fan state is controlled exclusively via web dashboard or API.  
Physical temperature readings still displayed; threshold still adjustable.

Toggle by pressing **UP + DOWN simultaneously** on the device.

---

## Software Setup

### Required Libraries (install via Arduino Library Manager)
| Library | Version |
|---|---|
| `DHT sensor library` by Adafruit | ≥ 1.4.6 |
| `Adafruit Unified Sensor` | ≥ 1.1.9 |
| `LiquidCrystal_I2C` by Frank de Brabander | ≥ 1.1.2 |
| ESP32 board support (via Boards Manager) | ≥ 2.0.0 |

### Board Settings in Arduino IDE
- Board: **ESP32 Dev Module**
- Upload Speed: **115200**
- Flash Size: **4MB (32Mb)**
- Partition Scheme: **Default 4MB with spiffs**

### Steps
1. Install all libraries above.
2. Open `fan_controller.ino`.
3. Replace `YOUR_SSID` and `YOUR_PASSWORD` with your WiFi credentials.
4. Select your COM port and click Upload.
5. Open Serial Monitor at 115200 baud to see the IP address.
6. Navigate to that IP in a browser.

---

## System Flow

```
Boot
 └─ Init LCD, DHT, Relay, WiFi
      └─ Connect WiFi → show IP on LCD
           └─ Start HTTP server

Loop (every 3 s)
 ├─ Read DHT11 → temperature, humidity
 ├─ Auto mode? → fan ON if temp ≥ threshold
 │              fan OFF if temp < threshold
 ├─ Manual mode? → fan state from remote/button override
 ├─ Drive RELAY_PIN (active-LOW)
 └─ Update LCD (every 1 s)

Buttons (polled, 250 ms debounce)
 ├─ UP   → threshold + 1 °C
 ├─ DN   → threshold − 1 °C
 ├─ SET  → persist to NVS flash
 └─ UP+DN → toggle Auto/Manual mode

HTTP server (async)
 ├─ GET /         → HTML dashboard
 ├─ GET /api/data → JSON sensor data
 └─ POST /api/set → update threshold / mode / fan
```

---

## Troubleshooting

| Issue | Check |
|---|---|
| DHT11 reads `nan` | Check DATA pin & 10kΩ pull-up resistor |
| LCD blank | Confirm I2C address (try 0x27 or 0x3F); check SDA/SCL wiring |
| Relay doesn't click | Verify 5V on relay VCC; GPIO26 should go LOW to activate |
| WiFi not connecting | Double-check SSID/password; ensure 2.4GHz network |
| Buttons not responding | GPIO 34/35 need external 10kΩ pull-up to 3.3V |
| Web page not loading | Check IP shown on LCD; must be on same WiFi network |

---

## Safety Notes

- Use a relay rated for your fan's voltage and current.
- For mains-voltage fans, consult an electrician; do **not** connect mains voltage to a breadboard.
- The ESP32 is a 3.3V device; do not apply 5V directly to GPIO pins.

---

*Project: Home Autonomous Fan Control System*  
*Platform: ESP32 · Sensors: DHT11 · Display: I2C LCD 16×2*

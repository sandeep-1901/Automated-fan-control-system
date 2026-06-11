/*
 * ============================================================
 *  Home Autonomous Fan Control System
 *  Hardware: ESP32 + DHT11 + Relay Module + I2C LCD + Buttons
 *  Author: Sandeep Thakur
 * ============================================================
 *
 *  PIN LAYOUT
 *  ──────────────────────────────────────
 *  DHT11 DATA        → GPIO 4
 *  RELAY IN          → GPIO 26  (active LOW)
 *  LCD SDA           → GPIO 21  (I2C default)
 *  LCD SCL           → GPIO 22  (I2C default)
 *  BTN_UP  (+1 °C)   → GPIO 34  (INPUT_PULLUP)
 *  BTN_DN  (−1 °C)   → GPIO 35  (INPUT_PULLUP)
 *  BTN_SET (save)    → GPIO 32  (INPUT_PULLUP)
 *  STATUS LED        → GPIO 2   (onboard LED)
 *  ──────────────────────────────────────
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <DHT.h>
#include <LiquidCrystal_I2C.h>

// ─── WiFi credentials ────────────────────────────────────────
const char* WIFI_SSID     = "YOUR_SSID";
const char* WIFI_PASSWORD = "YOUR_PASSWORD";

// ─── Pin definitions ─────────────────────────────────────────
#define DHT_PIN      4
#define DHT_TYPE     DHT11
#define RELAY_PIN    26
#define BTN_UP       34
#define BTN_DN       35
#define BTN_SET      32
#define LED_PIN      2

// ─── LCD: 16×2, I2C address 0x27 ─────────────────────────────
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ─── DHT sensor ──────────────────────────────────────────────
DHT dht(DHT_PIN, DHT_TYPE);

// ─── Web server on port 80 ───────────────────────────────────
WebServer server(80);

// ─── Persistent storage (survives reboots) ───────────────────
Preferences prefs;

// ─── State variables ─────────────────────────────────────────
float    temperature     = 0.0f;
float    humidity        = 0.0f;
int      threshold       = 28;      // °C default
bool     fanOn           = false;
bool     autoMode        = true;    // auto = threshold-based, manual = remote override
bool     manualFanState  = false;

// ─── Timing ──────────────────────────────────────────────────
unsigned long lastSensorRead  = 0;
unsigned long lastLcdUpdate   = 0;
unsigned long lastBtnPress    = 0;
const unsigned long SENSOR_INTERVAL = 3000;   // ms
const unsigned long LCD_INTERVAL    = 1000;
const unsigned long DEBOUNCE        = 250;

// ─── LCD custom characters ───────────────────────────────────
byte degreeChar[8] = {
  0b00110, 0b01001, 0b01001, 0b00110,
  0b00000, 0b00000, 0b00000, 0b00000
};
byte fanChar[8] = {
  0b00100, 0b10101, 0b01110, 0b11111,
  0b01110, 0b10101, 0b00100, 0b00000
};

// ─── Forward declarations ────────────────────────────────────
void readSensor();
void controlFan();
void updateLcd();
void handleButtons();
void setupWifi();
void setupWebServer();
void handleRoot();
void handleApi();
void handleSet();

// ─────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);

  pinMode(RELAY_PIN, OUTPUT);
  pinMode(LED_PIN,   OUTPUT);
  pinMode(BTN_UP,    INPUT_PULLUP);
  pinMode(BTN_DN,    INPUT_PULLUP);
  pinMode(BTN_SET,   INPUT_PULLUP);

  // Relay is active-LOW, start OFF (fan off)
  digitalWrite(RELAY_PIN, HIGH);
  digitalWrite(LED_PIN,   LOW);

  // LCD init
  lcd.init();
  lcd.backlight();
  lcd.createChar(0, degreeChar);
  lcd.createChar(1, fanChar);
  lcd.setCursor(0, 0);
  lcd.print(" Fan Controller ");
  lcd.setCursor(0, 1);
  lcd.print("  Booting...    ");

  // DHT init
  dht.begin();

  // Load saved threshold
  prefs.begin("fanctrl", false);
  threshold = prefs.getInt("threshold", 28);
  prefs.end();

  // Connect WiFi
  setupWifi();

  // Start web server
  setupWebServer();

  lcd.clear();
}

// ─────────────────────────────────────────────────────────────
void loop() {
  server.handleClient();
  handleButtons();

  unsigned long now = millis();

  if (now - lastSensorRead >= SENSOR_INTERVAL) {
    lastSensorRead = now;
    readSensor();
    controlFan();
  }

  if (now - lastLcdUpdate >= LCD_INTERVAL) {
    lastLcdUpdate = now;
    updateLcd();
  }
}

// ─── Sensor reading ──────────────────────────────────────────
void readSensor() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  if (!isnan(h) && !isnan(t)) {
    humidity    = h;
    temperature = t;
  } else {
    Serial.println("[DHT11] Read failed, retrying next cycle");
  }
}

// ─── Fan control logic ───────────────────────────────────────
void controlFan() {
  if (autoMode) {
    fanOn = (temperature >= (float)threshold);
  } else {
    fanOn = manualFanState;
  }

  // Active-LOW relay: LOW = relay energised = fan ON
  digitalWrite(RELAY_PIN, fanOn ? LOW : HIGH);
  digitalWrite(LED_PIN,   fanOn ? HIGH : LOW);
}

// ─── LCD update ──────────────────────────────────────────────
void updateLcd() {
  // Row 0: "T:27.5C  H:65%  "
  lcd.setCursor(0, 0);
  lcd.print("T:");
  lcd.print(temperature, 1);
  lcd.write(byte(0));   // degree symbol
  lcd.print("C H:");
  lcd.print((int)humidity);
  lcd.print("%  ");

  // Row 1: "Set:28C FAN:ON  " or "Set:28C FAN:--- "
  lcd.setCursor(0, 1);
  lcd.print("Set:");
  lcd.print(threshold);
  lcd.write(byte(0));
  lcd.print("C ");
  lcd.write(byte(1));   // fan icon
  lcd.print(fanOn ? "ON  " : "OFF ");
  lcd.print(autoMode ? "A" : "M");
  lcd.print("  ");
}

// ─── Button handler ──────────────────────────────────────────
void handleButtons() {
  unsigned long now = millis();
  if (now - lastBtnPress < DEBOUNCE) return;

  bool up  = (digitalRead(BTN_UP)  == LOW);
  bool dn  = (digitalRead(BTN_DN)  == LOW);
  bool set = (digitalRead(BTN_SET) == LOW);

  if (!up && !dn && !set) return;
  lastBtnPress = now;

  if (up && dn) {
    // Both UP+DN simultaneously → toggle auto/manual mode
    autoMode = !autoMode;
    Serial.printf("[BTN] Mode: %s\n", autoMode ? "AUTO" : "MANUAL");
    lcd.setCursor(0, 1);
    lcd.print(autoMode ? "  AUTO MODE     " : " MANUAL MODE    ");
    delay(700);
    return;
  }

  if (up) {
    threshold = min(threshold + 1, 50);
    Serial.printf("[BTN] Threshold UP → %d°C\n", threshold);
  }

  if (dn) {
    threshold = max(threshold - 1, 15);
    Serial.printf("[BTN] Threshold DN → %d°C\n", threshold);
  }

  if (set) {
    prefs.begin("fanctrl", false);
    prefs.putInt("threshold", threshold);
    prefs.end();
    Serial.printf("[BTN] Saved threshold: %d°C\n", threshold);
    // Brief confirmation on LCD
    lcd.setCursor(0, 1);
    lcd.print("  SAVED!        ");
    delay(500);
  }
}

// ─── WiFi setup ──────────────────────────────────────────────
void setupWifi() {
  lcd.setCursor(0, 1);
  lcd.print("WiFi connecting..");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int attempts = 0;

  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n[WiFi] Connected: %s\n", WiFi.localIP().toString().c_str());
    lcd.setCursor(0, 1);
    lcd.print(WiFi.localIP().toString());
  } else {
    Serial.println("\n[WiFi] Failed — running offline");
    lcd.setCursor(0, 1);
    lcd.print("WiFi FAILED     ");
    delay(1000);
  }
}

// ─── Web server routes ───────────────────────────────────────
void setupWebServer() {
  server.on("/",         HTTP_GET,  handleRoot);
  server.on("/api/data", HTTP_GET,  handleApi);
  server.on("/api/set",  HTTP_POST, handleSet);
  server.begin();
  Serial.println("[HTTP] Server started on port 80");
}

// ─── GET / → serve dashboard HTML ────────────────────────────
void handleRoot() {
  // Inline HTML dashboard (full page served from ESP32)
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Fan Control System</title>
<style>
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body { font-family: sans-serif; background: #0f1117; color: #e2e8f0; min-height: 100vh; padding: 20px; }
  h1 { text-align: center; font-size: 1.4rem; margin-bottom: 20px; color: #63b3ed; }
  .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(160px, 1fr)); gap: 14px; margin-bottom: 20px; }
  .card { background: #1a1f2e; border-radius: 12px; padding: 16px; border: 1px solid #2d3748; text-align: center; }
  .card .value { font-size: 2rem; font-weight: 600; }
  .card .label { font-size: 0.75rem; color: #a0aec0; margin-top: 4px; text-transform: uppercase; letter-spacing: 0.05em; }
  .temp .value { color: #fc8181; }
  .hum .value  { color: #63b3ed; }
  .fan .value  { font-size: 1.4rem; }
  .fan.on .value  { color: #68d391; }
  .fan.off .value { color: #a0aec0; }
  .thr .value  { color: #f6e05e; }
  .controls { background: #1a1f2e; border-radius: 12px; padding: 18px; border: 1px solid #2d3748; margin-bottom: 16px; }
  .controls h2 { font-size: 0.9rem; color: #a0aec0; margin-bottom: 14px; }
  .row { display: flex; align-items: center; gap: 10px; margin-bottom: 12px; }
  .row label { font-size: 0.85rem; color: #cbd5e0; min-width: 120px; }
  button { background: #2d3748; border: 1px solid #4a5568; color: #e2e8f0; border-radius: 8px; padding: 8px 16px; cursor: pointer; font-size: 0.85rem; }
  button:hover { background: #3a4558; }
  button.danger  { background: #c53030; border-color: #e53e3e; }
  button.success { background: #276749; border-color: #38a169; }
  input[type=number] { background: #2d3748; border: 1px solid #4a5568; color: #e2e8f0; border-radius: 6px; padding: 6px 10px; width: 70px; font-size: 0.9rem; }
  .badge { display: inline-block; padding: 2px 10px; border-radius: 999px; font-size: 0.75rem; font-weight: 600; }
  .badge.auto   { background: #276749; color: #9ae6b4; }
  .badge.manual { background: #744210; color: #fbd38d; }
  #status { font-size: 0.75rem; color: #68d391; text-align: center; margin-top: 10px; min-height: 18px; }
</style>
</head>
<body>
<h1>&#127908; Home Fan Control System</h1>

<div class="grid">
  <div class="card temp"><div class="value" id="temp">--</div><div class="label">Temperature (°C)</div></div>
  <div class="card hum"><div class="value" id="hum">--</div><div class="label">Humidity (%)</div></div>
  <div class="card fan off" id="fan-card"><div class="value" id="fan-state">---</div><div class="label">Fan Status</div></div>
  <div class="card thr"><div class="value" id="thr">--</div><div class="label">Threshold (°C)</div></div>
</div>

<div class="controls">
  <h2>Controls &nbsp; <span class="badge" id="mode-badge">AUTO</span></h2>
  <div class="row">
    <label>Threshold (°C)</label>
    <input type="number" id="thr-input" min="15" max="50" value="28">
    <button onclick="setThreshold()">Apply</button>
  </div>
  <div class="row">
    <label>Mode</label>
    <button onclick="setMode('auto')"  id="btn-auto">Auto</button>
    <button onclick="setMode('manual')" id="btn-manual">Manual</button>
  </div>
  <div class="row" id="manual-row" style="display:none">
    <label>Fan (manual)</label>
    <button class="success" onclick="setFan(true)">Turn ON</button>
    <button class="danger"  onclick="setFan(false)">Turn OFF</button>
  </div>
</div>
<div id="status"></div>

<script>
let currentMode = 'auto';

async function fetchData() {
  try {
    const r = await fetch('/api/data');
    const d = await r.json();
    document.getElementById('temp').textContent = d.temperature.toFixed(1);
    document.getElementById('hum').textContent  = d.humidity.toFixed(0);
    document.getElementById('thr').textContent  = d.threshold;
    document.getElementById('thr-input').value  = d.threshold;
    const fanCard  = document.getElementById('fan-card');
    const fanState = document.getElementById('fan-state');
    if (d.fan_on) { fanState.textContent = 'ON ✓'; fanCard.className='card fan on'; }
    else          { fanState.textContent = 'OFF';   fanCard.className='card fan off'; }
    currentMode = d.auto_mode ? 'auto' : 'manual';
    document.getElementById('mode-badge').textContent = currentMode.toUpperCase();
    document.getElementById('mode-badge').className   = 'badge ' + currentMode;
    document.getElementById('manual-row').style.display = currentMode === 'manual' ? 'flex' : 'none';
  } catch(e) { console.error(e); }
}

async function post(body) {
  const r = await fetch('/api/set', { method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify(body) });
  const d = await r.json();
  document.getElementById('status').textContent = d.message || 'Updated';
  setTimeout(() => document.getElementById('status').textContent = '', 3000);
  fetchData();
}

function setThreshold() { post({ threshold: parseInt(document.getElementById('thr-input').value) }); }
function setMode(m)      { post({ auto_mode: m === 'auto' }); }
function setFan(on)      { post({ manual_fan: on }); }

fetchData();
setInterval(fetchData, 3000);
</script>
</body>
</html>
)rawliteral";

  server.send(200, "text/html", html);
}

// ─── GET /api/data → JSON telemetry ──────────────────────────
void handleApi() {
  String json = "{";
  json += "\"temperature\":"   + String(temperature, 1) + ",";
  json += "\"humidity\":"      + String(humidity, 1)    + ",";
  json += "\"threshold\":"     + String(threshold)      + ",";
  json += "\"fan_on\":"        + (fanOn ? "true" : "false")    + ",";
  json += "\"auto_mode\":"     + (autoMode ? "true" : "false") + ",";
  json += "\"uptime_ms\":"     + String(millis());
  json += "}";
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", json);
}

// ─── POST /api/set → update settings ─────────────────────────
void handleSet() {
  String body = server.arg("plain");
  // Simple manual JSON parser (no library needed for these 3 keys)
  auto extractInt = [&](const char* key) -> int {
    String k = "\"" + String(key) + "\":";
    int idx = body.indexOf(k);
    if (idx < 0) return -9999;
    return body.substring(idx + k.length()).toInt();
  };
  auto extractBool = [&](const char* key) -> int {  // 1=true, 0=false, -1=absent
    String k = "\"" + String(key) + "\":";
    int idx = body.indexOf(k);
    if (idx < 0) return -1;
    String val = body.substring(idx + k.length(), idx + k.length() + 5);
    return val.startsWith("true") ? 1 : 0;
  };

  int  newThreshold = extractInt("threshold");
  int  newAutoMode  = extractBool("auto_mode");
  int  newFan       = extractBool("manual_fan");

  if (newThreshold != -9999) {
    threshold = constrain(newThreshold, 15, 50);
    prefs.begin("fanctrl", false);
    prefs.putInt("threshold", threshold);
    prefs.end();
  }
  if (newAutoMode != -1)  autoMode       = (newAutoMode == 1);
  if (newFan != -1)       manualFanState = (newFan == 1);

  controlFan();
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", "{\"ok\":true,\"message\":\"Settings updated\"}");
}

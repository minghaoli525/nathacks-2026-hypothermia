#include <Wire.h>
#include <U8g2lib.h>
#include "Adafruit_MPL3115A2.h"
#include "DFRobot_BloodOxygen_S.h"

// Map the breadboard switch, EMG input, and active-high alarm outputs to Uno pins.
constexpr uint8_t ButtonPin = A2;
constexpr uint8_t EmgPin = A0;
constexpr uint8_t BuzzerPin = 8;
constexpr uint8_t VibrationPin = 7;
// Preset three alarm inputs for the presentation. EMG still comes from A0.
constexpr bool DemoJWaveDetected = true;
constexpr float DemoTemperatureC = 8.0f;
constexpr int DemoBaselineBpm = 80;
constexpr int DemoHeartRateBpm = 60;
constexpr uint32_t OximeterPeriodMs = 4000;
constexpr uint8_t ScreenCount = 6;


// Use an eight-pixel-high OLED buffer to keep RAM use low on the Uno.
U8G2_SSD1306_128X64_NONAME_1_HW_I2C display(U8G2_R0, U8X8_PIN_NONE);
Adafruit_MPL3115A2 baro;
DFRobot_BloodOxygen_S_I2C oximeter(&Wire, 0x57);

uint8_t screen;
bool rawPressed, stablePressed;
uint32_t buttonChanged;
int emg;
bool emgOutsideRange;
uint32_t lastEmg;

// Cache optical readings between polls. A value of -1 marks an unavailable reading.
bool oximeterReady;
uint32_t lastOximeter;
int spo2 = -1;

// Keep track of the conversion in progress and the most recent valid environmental readings.
bool baroReady, conversionPending, haveBaroReading;
uint32_t lastBaro;
float altitude, pressure;
bool havePressure, pressureMode;

// Hold one frame's text and animation state steady while sending its pages in pieces.
const __FlashStringHelper *title;
char reading[17];
char detail[22];
bool animateBar;
uint8_t barWidth, dots;
constexpr uint8_t FrameChunks = 64;  // Render and transfer phases for eight pages.
uint8_t displayChunk = FrameChunks;
uint32_t lastDisplay;

void prepareDisplay() {
  detail[0] = '\0';
  animateBar = false;
  float value = 0;
  bool available = true;
  // Select one cached measurement in the same order the breadboard switch cycles through the pages.
  switch (screen) {
    case 0: title = F("EMG (DEMO)"); value = emg; break;
    case 1: title = F("Temp (SIM)"); value = DemoTemperatureC; break;
    case 2: title = F("Pressure"); value = pressure; available = havePressure; break;
    case 3: title = F("Altitude"); value = altitude; available = haveBaroReading; break;
    case 4:
      title = F("HR (SIM)");
      value = DemoHeartRateBpm;
      snprintf_P(detail, sizeof(detail), PSTR("Baseline %d BPM"), DemoBaselineBpm);
      break;
    case 5: title = F("SpO2"); value = spo2; available = spo2 > 0; break;
  }
  // Format a valid reading for the large text and advance the bottom animation.
  if (available) {
    dtostrf(value, 1, 2, reading);
    animateBar = true;
    barWidth = (barWidth + 10) % 70;
  } else {
    // Use a progress message and cycling dots while the selected reading is unavailable.
    reading[0] = '\0';
    dots = (dots + 1) % 4;
    strcpy_P(detail, PSTR("Taking measurement..."));
    detail[18 + dots] = '\0';
  }
}

void serviceDisplay(uint32_t now) {
  // Prepare a fresh snapshot only when all pages of the previous frame have been sent.
  if (displayChunk == FrameChunks) {
    if (now - lastDisplay < 200) return;
    lastDisplay = now;
    prepareDisplay();
    displayChunk = 0;
    return;
  }

  // Split each OLED page into separate drawing and transfer steps.
  const uint8_t page = displayChunk / 8;
  const uint8_t phase = displayChunk % 8;
  if (phase == 0) {
    // Move the drawing buffer to this page so only pixels in this eight-row band are retained.
    display.setBufferCurrTileRow(page);
    display.clearBuffer();
    display.setFont(u8g2_font_9x15B_tr);
    display.setCursor(0, 14);
    display.print(title);
  } else if (phase == 1) {
    display.setFont(u8g2_font_helvB18_tr);
    display.drawStr(0, 49, reading);
  } else if (phase == 2) {
    display.setFont(u8g2_font_5x8_tr);
    display.drawStr(0, 59, detail);
    if (animateBar) display.drawBox(64 - barWidth, 61, 2 * barWidth, 3);
  } else if (phase <= 6) {
    // Transfer only four tiles per pass so display updates leave time for other work.
    const uint8_t tile = (phase - 3) * 4;
    u8x8_DrawTile(display.getU8x8(), tile, page, 4, display.getBufferPtr() + tile * 8);
    // Restore the sensor bus speed after the faster OLED transfer.
    Wire.setClock(100000);
  }
  ++displayChunk;
}

void serviceButton(uint32_t now) {
  // The original A2 circuit reads below 300 ADC counts when the switch is closed.
  const bool pressed = analogRead(ButtonPin) < 300;
  if (pressed != rawPressed) {
    rawPressed = pressed;
    buttonChanged = now;
  }
  // Require a stable reading for 30 ms and advance once on the press edge.
  if (pressed != stablePressed && now - buttonChanged >= 30) {
    stablePressed = pressed;
    if (pressed) screen = (screen + 1) % ScreenCount;
  }
}

void readOximeter(uint32_t now) {
  lastOximeter = now;
  // The optional oximeter supplies the SpO2 page only.
  if (!oximeterReady) {
    oximeterReady = oximeter.begin();
    if (oximeterReady) oximeter.sensorStartCollect();
    return;
  }

  oximeter.getHeartbeatSPO2();
  const auto oxygen = oximeter._sHeartbeatSPO2.SPO2;
  spo2 = oxygen > 0 && oxygen <= 100 ? oxygen : -1;
}

void serviceBarometer(uint32_t now) {
  lastBaro = now;
  // Start a one-shot conversion and return while the rest of the loop continues.
  if (!conversionPending) {
    baro.setMode(pressureMode ? MPL3115A2_BAROMETER : MPL3115A2_ALTIMETER);
    conversionPending = baro.tryStartOneShot();
  // Once results are ready, update the cache and alternate pressure and altitude modes.
  } else if (baro.conversionComplete()) {
    if (pressureMode) {
      pressure = baro.getLastConversionResults(MPL3115A2_PRESSURE);
      havePressure = pressure > 0;
    } else {
      float unusedTemperature;
      haveBaroReading = baro.readAltitudeAndTemperature(altitude, unusedTemperature);
    }
    pressureMode = !pressureMode;
    conversionPending = false;
  }
}

void setup() {
  // Enable the switch pull-up and set both alarm pins low before initializing the sensors.
  pinMode(ButtonPin, INPUT_PULLUP);
  pinMode(BuzzerPin, OUTPUT);
  pinMode(VibrationPin, OUTPUT);
  digitalWrite(BuzzerPin, LOW);
  digitalWrite(VibrationPin, LOW);
  Wire.begin();
  // U8g2 takes the shifted form of the OLED's 7-bit I2C address.
  display.setI2CAddress(0x3C << 1);
  display.setBusClock(400000);
  display.begin();
  Wire.setClock(100000);
#if defined(WIRE_HAS_TIMEOUT)
  // Limit the wait for a stuck bus and let Wire reset its hardware when the timeout expires.
  Wire.setWireTimeout(1500, true);
#endif

  baroReady = baro.begin();
  // Load the existing sea-level pressure reference for the altitude calculation.
  if (baroReady) baro.setSeaPressure(1003.25);

  oximeterReady = oximeter.begin();
  if (oximeterReady) oximeter.sensorStartCollect();
  lastOximeter = millis();
}

void loop() {
  const uint32_t now = millis();

  // Read A0 at 20 Hz and flag values strictly below 450 or above 560.
  if (now - lastEmg >= 50) {
    lastEmg = now;
    emg = analogRead(EmgPin);
    emgOutsideRange = emg < 450 || emg > 580;
  }
  serviceButton(now);

  // Update the optional sensor pages and send OLED updates in short steps.
  if (now - lastOximeter >= OximeterPeriodMs) {
    readOximeter(now);
  } else if (baroReady && now - lastBaro >= (conversionPending ? 25 : 250)) {
    serviceBarometer(now);
  } else {
    serviceDisplay(now);
  }

  // These checks are preset to pass, leaving the live EMG to control the alarm.
  const bool lowHeartRate = DemoBaselineBpm - DemoHeartRateBpm >= 15;
  const bool cold = DemoTemperatureC < 10.0f;
  const bool alarm = DemoJWaveDetected && lowHeartRate && cold && emgOutsideRange;
  digitalWrite(BuzzerPin, alarm ? HIGH : LOW);
  digitalWrite(VibrationPin, alarm ? HIGH : LOW);
}

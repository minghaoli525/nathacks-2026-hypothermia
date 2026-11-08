#include <Wire.h>
#include <U8g2lib.h>
#include "Adafruit_MPL3115A2.h"
#include "DFRobot_BloodOxygen_S.h"
#include "Ecg.h"

// Map the breadboard switch, EMG input, and active-high alarm outputs to Uno pins.
constexpr uint8_t ButtonPin = A2;
constexpr uint8_t EmgPin = A0;
constexpr uint8_t BuzzerPin = 8;
constexpr uint8_t VibrationPin = 7;
// Set the cold threshold, reading freshness limit, and startup calibration window.
constexpr float ColdThresholdC = 10.0f;
constexpr uint32_t BaroFreshMs = 2000;
constexpr uint32_t BaselineMs = 30000;
constexpr uint32_t OximeterPeriodMs = 4000;
constexpr uint8_t ScreenCount = 6;


// Use an eight-pixel-high OLED buffer to leave RAM for the ECG detector.
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
int heartRate = -1, spo2 = -1;
// Collect a sum and sample count so the baseline needs no array of readings.
uint32_t baselineStart;
uint16_t baselineSum;
uint8_t baselineSamples;
int baselineBpm;
bool baselineReady;

// Keep track of the conversion in progress and the most recent valid environmental readings.
bool baroReady, conversionPending, haveBaroReading;
uint32_t lastBaro;
uint32_t lastBaroReading;
float temperature, altitude, pressure;
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

void prepareDisplay(uint32_t now) {
  detail[0] = '\0';
  animateBar = false;
  // Show the remaining calibration time before exposing the measurement pages.
  if (!baselineReady) {
    title = F("HR baseline");
    const uint32_t elapsed = min(now - baselineStart, BaselineMs);
    snprintf_P(reading, sizeof(reading), PSTR("%u s"), unsigned((BaselineMs - elapsed + 999) / 1000));
    strcpy_P(detail, oximeterReady ? PSTR("Hold finger still") : PSTR("Check oximeter"));
    return;
  }

  float value = 0;
  bool available = true;
  // Select one cached measurement in the same order the breadboard switch cycles through the pages.
  switch (screen) {
    case 0: title = F("EMG"); value = emg; break;
    case 1: title = F("Temperature"); value = temperature; available = haveBaroReading; break;
    case 2: title = F("Pressure"); value = pressure; available = havePressure; break;
    case 3: title = F("Altitude"); value = altitude; available = haveBaroReading; break;
    case 4:
      title = F("Heart rate");
      value = heartRate;
      available = heartRate > 0;
      snprintf_P(detail, sizeof(detail), PSTR("Baseline %d BPM"), baselineBpm);
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
    prepareDisplay(now);
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
    // Transfer only four tiles per pass to leave time for ECG sampling.
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
    if (pressed && baselineReady) screen = (screen + 1) % ScreenCount;
  }
}

void readOximeter(uint32_t now) {
  lastOximeter = now;
  // Retry the module probe at the polling interval and restart calibration if it comes online.
  if (!oximeterReady) {
    oximeterReady = oximeter.begin();
    if (oximeterReady) {
      oximeter.sensorStartCollect();
      if (!baselineReady) {
        baselineStart = now;
        baselineSum = baselineSamples = 0;
      }
    }
    return;
  }

  // Fetch both optical readings together, then reject values outside their accepted ranges.
  oximeter.getHeartbeatSPO2();
  const auto bpm = oximeter._sHeartbeatSPO2.Heartbeat;
  const auto oxygen = oximeter._sHeartbeatSPO2.SPO2;
  heartRate = bpm > 0 && bpm <= 255 ? bpm : -1;
  spo2 = oxygen > 0 && oxygen <= 100 ? oxygen : -1;
  // Accumulate valid heart-rate readings that arrive within the current calibration window.
  if (!baselineReady && heartRate > 0 && now - baselineStart < BaselineMs) {
    baselineSum += heartRate;
    ++baselineSamples;
  }
}

void finishBaseline(uint32_t now) {
  if (baselineReady || now - baselineStart < BaselineMs) return;
  // Round the average to the nearest whole BPM, or start a fresh window if too few readings arrived.
  if (baselineSamples >= 3) {
    baselineBpm = (baselineSum + baselineSamples / 2) / baselineSamples;
    baselineReady = true;
  } else {
    baselineStart = now;
    baselineSum = baselineSamples = 0;
  }
}

void serviceBarometer(uint32_t now) {
  lastBaro = now;
  // Start a one-shot conversion and return so ECG sampling can continue while the sensor works.
  if (!conversionPending) {
    baro.setMode(pressureMode ? MPL3115A2_BAROMETER : MPL3115A2_ALTIMETER);
    conversionPending = baro.tryStartOneShot();
  // Once results are ready, update the cache and alternate pressure and altitude modes.
  } else if (baro.conversionComplete()) {
    if (pressureMode) {
      pressure = baro.getLastConversionResults(MPL3115A2_PRESSURE);
      havePressure = pressure > 0;
    } else {
      haveBaroReading = baro.readAltitudeAndTemperature(altitude, temperature);
      if (haveBaroReading) lastBaroReading = now;
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
  // Start the baseline clock and ECG schedule after the hardware has been initialized.
  baselineStart = lastOximeter = millis();
  beginEcg();
}

void loop() {
  // Give the 250 Hz ECG sampler first use of each loop iteration.
  if (!serviceEcg()) return;
  const uint32_t now = millis();
  finishBaseline(now);

  // Read A0 at 20 Hz and flag values strictly below 450 or above 560.
  if (now - lastEmg >= 50) {
    lastEmg = now;
    emg = analogRead(EmgPin);
    emgOutsideRange = emg < 450 || emg > 560;
  }
  serviceButton(now);

  // Schedule one I2C job per sample to leave time for the next ECG reading.
  if (now - lastOximeter >= OximeterPeriodMs) {
    readOximeter(now);
  } else if (baroReady && now - lastBaro >= (conversionPending ? 25 : 250)) {
    serviceBarometer(now);
  } else {
    serviceDisplay(now);
  }

  // Require a valid live heart rate at least 15 BPM below the completed baseline.
  const bool lowHeartRate = baselineReady && heartRate > 0 &&
                            baselineBpm - heartRate >= 15;
  // Require a fresh temperature reading strictly below 10 degrees Celsius.
  const bool cold = haveBaroReading && now - lastBaroReading < BaroFreshMs &&
                    temperature < ColdThresholdC;
  // Drive both outputs together only when the J-wave hold is active and all three sensor checks agree.
  const bool alarm = jWaveAlarmActive() && lowHeartRate && emgOutsideRange && cold;
  digitalWrite(BuzzerPin, alarm ? HIGH : LOW);
  digitalWrite(VibrationPin, alarm ? HIGH : LOW);
}

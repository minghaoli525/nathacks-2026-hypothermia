// Standalone presentation test. Only the live EMG input and LED are used.
constexpr uint8_t EmgPin = A0;
constexpr uint8_t LedPin = 8;
constexpr int EmgLow = 450;
constexpr int EmgHigh = 560;
constexpr unsigned long SamplePeriodMs = 50;

void setup() {
  pinMode(LedPin, OUTPUT);
  digitalWrite(LedPin, LOW);
}

void loop() {
  // Sample the EMG at 20 Hz, as in the presentation sketch.
  const int emg = analogRead(EmgPin);

  // Light the LED outside the inclusive 450-560 range.
  const bool outsideRange = emg < EmgLow || emg > EmgHigh;
  digitalWrite(LedPin, outsideRange ? HIGH : LOW);
  delay(SamplePeriodMs);
}

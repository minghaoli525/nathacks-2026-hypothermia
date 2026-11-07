// Fixed Sampling - BioAmp EXG Pill (ESP32 3.3V Version)
// https://github.com/upsidedownlabs/BioAmp-EXG-Pill

// Samples per second
#define SAMPLE_RATE 20

// Set the same baud rate on your Serial Monitor/Plotter
#define BAUD_RATE 115200

// ESP32 ADC1 Pin (GPIO 34, 35, 36/VP, or 39/VN)
#define INPUT_PIN 34

void setup() {
  // Initialize serial communication
  Serial.begin(BAUD_RATE);

  // Optional: Set ADC to 10-bit resolution (0-1023) to match Arduino UNO range.
  // Uncomment the line below if required by your visualization software:
  // analogReadResolution(10);
}

void loop() {
  // Calculate elapsed time
  static unsigned long past = 0;
  unsigned long present = micros();
  unsigned long interval = present - past;
  past = present;

  // Run timer
  static long timer = 0;
  timer -= interval;

  // Sample
  if (timer < 0) {
    timer += 1000000 / SAMPLE_RATE;
    int sensor_value = analogRead(INPUT_PIN);
    Serial.print(millis()); // Print time (ms)
    Serial.print(",");      // Comma separator
    Serial.println(sensor_value);
  }
}


#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SAMPLE_RATE 20
#define BAUD_RATE 115200
#define INPUT_PIN A0

// Buzzer and vibration motor
#define BUZZER_PIN 8
#define VIBRATION_PIN 7

// EMG thresholds
#define LOW_THRESHOLD 350
#define HIGH_THRESHOLD 680

// OLED screen
Adafruit_SSD1306 display(128, 64, &Wire, -1);

int sensor_value = 0;

void setup() {
  Serial.begin(BAUD_RATE);
  Wire.begin();

  // Set buzzer pin
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // Set vibration motor pin
  pinMode(VIBRATION_PIN, OUTPUT);
  digitalWrite(VIBRATION_PIN, LOW);

  // Start OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED failed!");
    while (true);
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.display();
}

void loop() {

  // Read EMG sensor
  sensor_value = analogRead(INPUT_PIN);

  // Send value to Serial Monitor
  Serial.println(sensor_value);

  // Check EMG value
  bool warning = sensor_value < LOW_THRESHOLD ||
                 sensor_value > HIGH_THRESHOLD;

  // Control buzzer and vibration motor
  if (warning) {
    digitalWrite(BUZZER_PIN, HIGH);
    digitalWrite(VIBRATION_PIN, HIGH);
  } else {
    digitalWrite(BUZZER_PIN, LOW);
    digitalWrite(VIBRATION_PIN, LOW);
  }

  // Update OLED
  display.clearDisplay();

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("EMG Monitor");

  display.setTextSize(2);
  display.setCursor(0, 20);
  display.print(sensor_value);

  display.setTextSize(1);
  display.setCursor(0, 50);

  if (warning) {
    display.println("WARNING!");
  } else {
    display.println("NORMAL");
  }

  display.display();

  // Sampling interval
  delay(1000 / SAMPLE_RATE);
}

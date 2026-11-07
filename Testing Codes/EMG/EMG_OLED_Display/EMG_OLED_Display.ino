
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SAMPLE_RATE 20
#define BAUD_RATE 115200
#define INPUT_PIN A0

// OLED screen
Adafruit_SSD1306 display(128, 64, &Wire, -1);

int sensor_value = 0;

void setup() {
  Serial.begin(BAUD_RATE);
  Wire.begin();

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
  static unsigned long lastSample = 0;
  static unsigned long lastDisplay = 0;

  // Read EMG 20 times per second
  if (micros() - lastSample >= 50000UL) {
    lastSample = micros();

    sensor_value = analogRead(INPUT_PIN);

    Serial.print(millis());
    Serial.print(",");
    Serial.println(sensor_value);
  }

  // Update OLED 5 times per second
  if (millis() - lastDisplay >= 200) {
    lastDisplay = millis();

    display.clearDisplay();

    display.setTextSize(2);
    display.setCursor(0, 0);
    display.println("EMG:");

    display.setTextSize(3);
    display.setCursor(0, 28);
    display.println(sensor_value);

    display.display();
  }
}

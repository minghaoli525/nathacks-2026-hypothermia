#include <Wire.h>
#include "DFRobot_BloodOxygen_S.h"

DFRobot_BloodOxygen_S_I2C sensor(&Wire, 0x57);

void setup() {
  Serial.begin(115200);

  while (!sensor.begin()) {
    Serial.println("Sensor not found. Check wiring and the I2C switch.");
    delay(1000);
  }

  sensor.sensorStartCollect();
  Serial.println("Place your finger on the sensor and hold still.");
}

void loop() {
  // delay(3000);  // The module updates about every four seconds.
  sensor.getHeartbeatSPO2();

  Serial.print("Heart rate: ");
  Serial.print(sensor._sHeartbeatSPO2.Heartbeat);
  Serial.print(" BPM | SpO2: ");
  Serial.print(sensor._sHeartbeatSPO2.SPO2);
  Serial.println("%");
}

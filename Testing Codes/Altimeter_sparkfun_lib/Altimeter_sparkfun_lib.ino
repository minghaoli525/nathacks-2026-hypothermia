#include <Wire.h>
#include "SparkFunMPL3115A2.h"

/* Connexions for Arduino nano*/
//push button -> D4
/* 7 digits diplay TM1637 */
//DIO -> D5
//CLK -> D7
/* pressure sensor MPL3115A2 */
//SCL -> A5
//SDA -> 4A

MPL3115A2 baro;
int altitude_max;
int altitude_offset=0;

void setup () {
  pinMode(4, INPUT); //push button
  
          // Join i2c bus
  Serial.begin(115200);  // Start serial for output

  baro.begin(); // Get sensor online

  //Configure the sensor
  baro.setModeAltimeter(); // Measure altitude above sea level in meters

  baro.setOversampleRate(7); // Set Oversample to the recommended 128
  baro.enableEventFlags(); // Enable all three pressure and temp event flags 
}

//wire SDA to D21
//SCL to D22
//Vin 5V


void loop () {      
  unsigned long time = micros();
  
  double temp = baro.readTemp();
  // float press = baro.readPressure(); //Set to baro.setModeAltimeter() to baro.setModeBarometer() to use pressure, then altimeter stop working
  double altitude = baro.readAltitude();

  Serial.print("Altitude(cm):");
  Serial.print(altitude);
  Serial.println();

  Serial.print("Altitude time:");
  unsigned long time1 = micros();
  Serial.println(time1-time);

  // Serial.print("Pressure(Pa):");
  // Serial.print(press);
  // Serial.println();


  // Serial.print("Pressure time:");
  // time = micros();
  // Serial.println(time-time1);
  
  Serial.print("temp(C):");
  Serial.print(temp);
  Serial.println();

  
  Serial.print("temp time:");
  time1 = micros();
  Serial.println(time1-time);
}

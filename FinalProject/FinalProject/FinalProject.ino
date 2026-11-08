
#include <Wire.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
Adafruit_SSD1306 display(128, 64, &Wire, -1);
int mode = 0;
#define TOTALMODECOUNT 6

#define BUTTON A2
int button = 0;
bool on = false;
int button1=0;

#define INPUT_PIN A0
unsigned long lastDisplay = millis();
int sensor_value = 400;
unsigned long lastSample = millis();

#include "Adafruit_MPL3115A2.h"
Adafruit_MPL3115A2 baro;

#define BUZZER 8

#include "DFRobot_BloodOxygen_S.h"
DFRobot_BloodOxygen_S_I2C sensor(&Wire, 0x57);

// int i =0;

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON, INPUT_PULLUP); //button

  Wire.begin();

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.display();

  baro.begin();
  baro.setSeaPressure(1013.25);

  sensor.begin();
  sensor.sensorStartCollect();

  pinMode(BUZZER, OUTPUT);
}

void loop() {
  mode = ButtonSwitch(mode);

  // if (i>50){
    
  //   if ( mode <4){
  //   mode += 1;}
  //   else{
  //     mode = 0;
  //   }
  //   i=0;
  // }
  // else {i+=1;}

  //sensor_value = TakeEMGreading(sensor_value);

  sensor.getHeartbeatSPO2();

  if (sensor_value< 1500){
    sensor_value += 1;
  }
  else {
    sensor_value = 0;
  }

  delay(10);
  if (sensor_value < 350 || sensor_value >950){
    analogWrite(BUZZER,150);
    delay(5);
    analogWrite(BUZZER,0);
    delay(10);
  }
  else{
    analogWrite(BUZZER, 0);
  }
  //display EMG
  if (mode== 0){
    ShowOnScreen(sensor_value,"EMG:");
    // Serial.print(millis());
    // Serial.print(",");
    // Serial.println(sensor_value);
  }

  //display temperature
  if (mode == 1){
    ShowOnScreen(baro.getTemperature(),"Temperature:");
    Serial.print("temperature = "); Serial.print(baro.getTemperature()); Serial.println(" C");
  }

  if (mode == 2){
    ShowOnScreen(baro.getPressure(),"Pressure:");
    Serial.print("pressure = "); Serial.print(baro.getPressure()); Serial.println(" hPa");
  }

  if (mode == 3){
    ShowOnScreen(baro.getAltitude(),"Altitude:");
    Serial.print("altitude = "); Serial.print(baro.getAltitude()); Serial.println(" m");
  }

  if (mode == 4){
    ShowOnScreen(sensor._sHeartbeatSPO2.Heartbeat,"Heartbeat:");
    Serial.print("heartbeat = "); Serial.print(sensor._sHeartbeatSPO2.Heartbeat); Serial.println(" bpm");
  }

  if (mode == 5){
    ShowOnScreen(sensor._sHeartbeatSPO2.SPO2,"SP02:");
    Serial.print("SPO2 = "); Serial.print(sensor._sHeartbeatSPO2.SPO2); Serial.println(" %");
  }
}



int ButtonSwitch(int mode){
  
  Serial.println(button);
  
  if (button>0){
    button1 = 1;
    Serial.println("button>0");
  }
  else {
    button1 = 0;
  }
  button=analogRead(BUTTON)-300;
  // open button ~= 700 
  // closed button ~= 250
  
  if (button1==1 && button<0 && mode < TOTALMODECOUNT ){
    mode += 1;}
  else if (button1==1 && button<0) {
    mode = 0;
  }
  Serial.println(mode);
  return mode;

}

int TakeEMGreading(int sensor_value){
  // Read EMG 20 times per second
  if (millis() - lastSample >= 50) {
    lastSample = millis();

    sensor_value = analogRead(INPUT_PIN);
  }
  return sensor_value;
}

void ShowOnScreen(float value, char MeasurementName[10]){
  // Update OLED 5 times per second
  if (millis() - lastDisplay >= 200) {
    lastDisplay = millis();

    display.clearDisplay();

    display.setTextSize(2);
    display.setCursor(0, 0);
    display.setTextWrap(false);
    display.println(MeasurementName);


    display.setTextSize(3);
    display.setCursor(0, 28);
    if (value!=-1){
      display.println(value);
    }
    else {
      display.setTextSize(1);
      display.println("taking measurement...");
    }
    display.display();
  }
}
/*
 * TechsPassion - ESP32 Over The Air (OTA) Upload
 * Upload once over USB, then upload new code over WiFi.
 */

//#include <WiFi.h>
//#include <ArduinoOTA.h>

// WiFi credentials (replace with your actual credentials)
const char* ssid = "SM-VincentNgo-Khai:))";
const char* password = "1234567890@1";

const int ledPin = 48;     // Colour LED on ESP32-S3 boards
const int blinkMs = 1000;  // Change this, then upload over WiFi to see the update

void setup() {
  Serial.begin(115200);
  pinMode(13,OUTPUT);
  
  // Connect to WiFi
//  WiFi.begin(ssid, password);
//  while (WiFi.status() != WL_CONNECTED) {
//    delay(500);
//    Serial.print(".");
//    digitalWrite(13,HIGH);
//    delay(500);
//    digitalWrite(13,LOW);
//    delay(500);
//  }
//  Serial.println();
//  Serial.print("IP address: ");
//  Serial.println(WiFi.localIP());

  // Start OTA: the board shows up in Tools > Port with this name
//  ArduinoOTA.setHostname("Nathan Lin");
//  ArduinoOTA.setPassword("LinLin");  // The IDE asks for this when uploading
//  ArduinoOTA.begin();
  
}

void loop() {
  //ArduinoOTA.handle();  // Listens for new code, keep loop() free of long delays

  // Blink the LED blue without delay()
  digitalWrite(13,HIGH);
  //#delay(1500);
 
  //#delay(200);

  float val = analogRead(34);
  Serial.print("Analog value: ");
  Serial.println(ECGFilter(val));
  delay(100);
  digitalWrite(13,LOW);

}

float ECGFilter(float input) {
  float output = input;
  {
    static float z1, z2;  // filter section state
    float x = output - 0.70682283 * z1 - 0.15621030 * z2;
    output = 0.28064917 * x + 0.56129834 * z1 + 0.28064917 * z2;
    z2 = z1;
    z1 = x;
  }
  {
    static float z1, z2;  // filter section state
    float x = output - 0.95028224 * z1 - 0.54073140 * z2;
    output = 1.00000000 * x + 2.00000000 * z1 + 1.00000000 * z2;
    z2 = z1;
    z1 = x;
  }
  {
    static float z1, z2;  // filter section state
    float x = output - -1.95360385 * z1 - 0.95423412 * z2;
    output = 1.00000000 * x + -2.00000000 * z1 + 1.00000000 * z2;
    z2 = z1;
    z1 = x;
  }
  {
    static float z1, z2;  // filter section state
    float x = output - -1.98048558 * z1 - 0.98111344 * z2;
    output = 1.00000000 * x + -2.00000000 * z1 + 1.00000000 * z2;
    z2 = z1;
    z1 = x;
  }
  return output;
}
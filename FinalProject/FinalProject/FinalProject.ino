
#include <Adafruit_SSD1306.h>
Adafruit_SSD1306 display(128, 64, &Wire, -1);
int mode = 0;

#define BUTTON 17
int button = 0;
bool on = false;
int button1;



void setup() {
  Serial.begin(115200);
  pinMode(BUTTON, INPUT); //button





}

void loop() {

  ButtonSwitch();



}



bool ButtonSwitch(){
  button1 = button;
  button=digitalRead(BUTTON);
  Serial.println(button);
  if (button1==1 && button==0){
    on = !on;}
  return on;
}



void ShowOnScreen(){

}
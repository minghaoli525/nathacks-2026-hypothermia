int button = 0;
bool on = false;
int button1;
void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  pinMode(13,OUTPUT);
  pinMode(38, INPUT);
  
}

void loop() {
  // put your main code here, to run repeatedly:
  
  TurnVibratorOnOff(ButtonSwitch());
  
}

bool ButtonSwitch(){
  button1 = button;
  button=digitalRead(38);
  if (button1==1 && button==0){
    on = !on;}
  return on;
}

void TurnVibratorOnOff(bool a){
  if (a){
    analogWrite(13,100); //max 255
    Serial.println("on");
  
  }
  else{
    Serial.println("off");
    analogWrite(13,0);}
}


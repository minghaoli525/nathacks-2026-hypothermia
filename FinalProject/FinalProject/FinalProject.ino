
int button = 0;
bool on = false;
int button1;



void setup() {

  pinMode(38, INPUT);





}

void loop() {


}



bool ButtonSwitch(){
  button1 = button;
  button=digitalRead(38);
  if (button1==1 && button==0){
    on = !on;}
  return on;
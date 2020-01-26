int analogPin0 = A0; 
int analogPin1 = A1;
int analogPin2 = A2;

int i = 0;
bool on = false;

int val0 = 0;
int val1 = 0;
int val2 = 0;

void setup() {
  Serial.begin(9600);           //  setup serial
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.println("start"); 
  delay(200);  
}

void loop() {
  blink();
  readVals();
  writeVals();
//  debugPrint();
}

void readVals() {
  val0 = analogRead(analogPin0);  
  val1 = analogRead(analogPin1);  
  val2 = analogRead(analogPin2);  
}

void writeVals() {
  Serial.print("X");
  Serial.print(val0);
  Serial.print("Y");
  Serial.print(val1);
  Serial.print("Y");
  Serial.print(val2);
}

void debugPrint() {
  Serial.print(val0);
  Serial.print(", ");
  Serial.print(val1);
  Serial.print(", ");
  Serial.print(val2);
  Serial.println();
}

void blink() {
  if(i++ == 100) {
    digitalWrite(LED_BUILTIN, on ? HIGH : LOW);
    on = !on;
    i = 0;
  }
}

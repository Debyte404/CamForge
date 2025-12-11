#include<Arduino.h>
#define BTN_A 27   // change this to your actual pin number

void setup() {
  Serial.begin(115200);
  pinMode(BTN_A, INPUT_PULLUP);
}

void loop() {
  int state = digitalRead(BTN_A);
  Serial.println(state == LOW ? "Pressed" : "Released");
  delay(300);
}

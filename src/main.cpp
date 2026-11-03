#include <Arduino.h>
#include "board_pins.h"

void setup() {
  pinMode(PIN_RT9080_EN, OUTPUT);
  digitalWrite(PIN_RT9080_EN, HIGH);
  Serial.begin(115200);
  Serial.println("NETH3T boot");
}

void loop() {}

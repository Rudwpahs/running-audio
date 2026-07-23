#include <Arduino.h>

#include "pr1_config.h"

void setupRole();
void loopRole();

void setup() {
  pinMode(PR1_STATUS_LED, OUTPUT);
  digitalWrite(PR1_STATUS_LED, LOW);
  setupRole();
}

void loop() {
  loopRole();
}

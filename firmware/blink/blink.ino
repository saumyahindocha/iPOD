// Pod: blink test
// Blinks the Pico 2 W's onboard LED twice a second.
// If this works, the build pipeline and flashing are working.

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
  digitalWrite(LED_BUILTIN, HIGH);
  delay(250);
  digitalWrite(LED_BUILTIN, LOW);
  delay(250);
}

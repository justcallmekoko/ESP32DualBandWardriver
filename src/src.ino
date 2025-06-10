void setup() {
  Serial.begin(115200);

  while (!Serial)
    delay(10);

  Serial.println("Started successfully!");

}

void loop() {
  Serial.println(millis());

  delay(1000);

}

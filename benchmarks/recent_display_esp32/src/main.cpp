#include "allocations.h"
void setup() {
  Serial.begin(115200);
  setCpuFrequencyMhz(240);
  delay(500);
  Serial.printf("READY,%s\n", benchmark_revision);
  while (!Serial.available()) delay(10);
  int command = Serial.read();
  if (command == 'S') RunCompositionBenchmark();
  if (command == 'W') RunWifiBenchmark();
}
void loop() { delay(1000); }

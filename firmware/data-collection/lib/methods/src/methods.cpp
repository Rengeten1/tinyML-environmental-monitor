#include <Arduino.h>
#include <DHT.h>

#include "methods.h"
#include "../../pinConfig/pinConfig.h"

static DHT dht(DHTPIN, DHTTYPE);

void initSensors() {
  dht.begin();
}

void outputData() {
  const float humidity = dht.readHumidity();
  const float temperatureC = dht.readTemperature(false); // false = Celsius

  if (isnan(humidity) || isnan(temperatureC)) {
    Serial.println("NaN, NaN");
    return;
  }

  Serial.print(humidity, 2);
  Serial.print(", ");
  Serial.println(temperatureC, 2);
}

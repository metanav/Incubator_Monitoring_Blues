#include <Notecard.h>

#include "Adafruit_SGP30.h"
#include "Adafruit_SHT4x.h"

#define LEDB 25
#define LEDG 16
#define LEDR 17
#define PRODUCT_UID "com.xxx.xxxx:incubator"
#define myProductID PRODUCT_UID

Notecard notecard;

Adafruit_SHT4x sht4 = Adafruit_SHT4x();
Adafruit_SGP30 sgp;

/* return absolute humidity [mg/m^3] with approximation formula
  @param temperature [°C]
  @param humidity [%RH] !`
*/
uint32_t getAbsoluteHumidity(float temperature, float humidity)
{
  // approximation formula from Sensirion SGP30 Driver Integration chapter 3.15
  const float absoluteHumidity = 216.7f * ((humidity / 100.0f) * 6.112f * exp((17.62f * temperature) / (243.12f + temperature)) / (273.15f + temperature)); // [g/m^3]
  const uint32_t absoluteHumidityScaled = static_cast<uint32_t>(1000.0f * absoluteHumidity); // [mg/m^3]

  return absoluteHumidityScaled;
}

void errorLoop(unsigned int delay_ms)
{
  while (1) {
    digitalWrite(LEDR, LOW);
    delay(delay_ms);
    digitalWrite(LEDR, HIGH);
    delay(delay_ms);
    Serial.println("Error");
  }
}


void setup()
{  
  // keep LEDs off
  pinMode(LEDR, OUTPUT);
  pinMode(LEDG, OUTPUT);
  pinMode(LEDB, OUTPUT);

  digitalWrite(LEDR, HIGH);
  digitalWrite(LEDG, HIGH);
  digitalWrite(LEDB, HIGH);

  if (! sgp.begin()) {
    Serial.println("SGP30 not found :(");
    errorLoop(500);
  }

  if (! sht4.begin()) {
    Serial.println("Couldn't find SHT40");
    errorLoop(500);
  }

  sht4.setPrecision(SHT4X_HIGH_PRECISION);
  sht4.setHeater(SHT4X_NO_HEATER);

  notecard.begin();
  J *req = notecard.newRequest("hub.set");
  if (myProductID[0])
  {
    JAddStringToObject(req, "product", myProductID);
  }
  JAddStringToObject(req, "mode", "continuous");
  notecard.sendRequestWithRetry(req, 5);
}

void loop()
{
  // SHT30
  sensors_event_t humidity, temp;
  sht4.getEvent(&humidity, &temp);

  // SGP40
  sgp.setHumidity(getAbsoluteHumidity(temp.temperature, humidity.relative_humidity));
  if (! sgp.IAQmeasure()) {
    Serial.println("Measurement failed");
    return;
  }

  J *req = notecard.newRequest("note.add");

  if (req != NULL) {
    JAddBoolToObject(req, "sync", true);
    J *body = JAddObjectToObject(req, "body");
    if (body != NULL) {
      JAddNumberToObject(body, "temperature", temp.temperature);
      JAddNumberToObject(body, "humidity", humidity.relative_humidity);
      JAddNumberToObject(body, "TVOC", sgp.TVOC);
      JAddNumberToObject(body, "eCO2", sgp.eCO2);
    }
    notecard.sendRequest(req);
  }
  delay(60UL * 60UL * 1000UL); // every hour
}

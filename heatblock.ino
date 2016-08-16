// I need that block to heat up to 63 degrees for half an our and then to 93 for 10 min. That's all really.

#define STAGE1TEMP      63 // temperature for first stage
#define STAGE1TIME      (30*60000) // minutes times seconds times milliseconds
#define STAGE2TEMP      93 // temperature for second stage
#define STAGE2TIME      (10*60000) // minutes times seconds times milliseconds

#include <OneWire.h>
#include <Adafruit_NeoPixel.h>

#define DS18S20_Pin     2 // DS18S20 Signal pin on digital 2
#define BEEPER_PIN      4 // make soothing noises for the yumens
#define LEDSTRIP_PIN    5 // string of WS2812B LEDs for tub status display
#define LEDSTRIP_LEDS   10 // how many LEDs on the strip
#define HEATER_PIN      7 // to turn on heater
#define STATUS_LED_PIN  8 // on when heating, blinking when finished

#define TEMP_VALID_MIN  -10 // below this temperature is considered an invalid reading
#define TEMP_VALID_MAX  200 // above this temp is considered an invalid reading
#define NOHEAT         -200 // set celsiusTarget to this if we don't want heat

#include "DS18S20.h" // reads temperature from the one digital temp sensor

float celsiusReading; // stores valid value read from temp sensor
float celsiusTarget = NOHEAT; // target heating temperature
byte stage = 0; // what stage are we on?
unsigned long time,targetTime,lastTempReading;
Adafruit_NeoPixel LEDStrip = Adafruit_NeoPixel(LEDSTRIP_LEDS, LEDSTRIP_PIN, NEO_GRB + NEO_KHZ800);

void setLEDStrip(byte r, byte g, byte b) {
  for(byte i=0; i<LEDStrip.numPixels(); i++) {
    LEDStrip.setPixelColor(i, LEDStrip.Color(r,g,b));
    LEDStrip.show();
  }
}

void printTime(unsigned long time) {
  Serial.print(time/60000); // time since boot in minutes
  Serial.print(":");
  byte seconds = (time%60000)/1000;
  if (seconds < 10) Serial.print("0");
  Serial.print(seconds); // and seconds
}

void setup() {
  tone(BEEPER_PIN, 800, 1000); // make a beep (non-blocking function)
  pinMode(HEATER_PIN, OUTPUT);
  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN,HIGH); // turn status LED on
  Serial.begin(57600);
  Serial.println("\nheatblock");
  while(! initTemp()){ // can't do anything without the temperature sensor
    printTime(millis()); // print time since boot
    Serial.println("    ERROR: DS18B20 temp sensor NOT found!!!");
    setLEDStrip(255,0,0); // red
    delay(500);
    setLEDStrip(20,0,0); // dim red
    delay(500);
  }
  for (byte i=0; i<8; i++) Serial.print(DS18S20addr[i],HEX); // print address
  Serial.print("  DS18B20 temp sensor found, degrees C = ");
  Serial.println(getTemp());
  LEDStrip.begin(); // init LED strip
  setLEDStrip(0,0,255); // set to blue
}

void loop() {
  time = millis();
  switch(stage) {
    case 0:
      stage = 1;
      celsiusTarget = STAGE1TEMP;
      targetTime = 0; // don't set stoptime until target is reached
      break;
    case 1:
      if (!targetTime) { // if targetTime hasn't been set yet and is 0
        if (celsiusReading > celsiusTarget) targetTime = time + STAGE1TIME;
      } else {
        if (time > targetTime) {
          stage = 2;
          celsiusTarget = STAGE2TEMP;
          targetTime = 0; // don't set stoptime until target is reached
        }
      }
      break;
    case 2:
      if (!targetTime) { // if targetTime hasn't been set yet and is 0
        if (celsiusReading > celsiusTarget) targetTime = time + STAGE2TIME;
      } else {
        if (time > targetTime) {
          stage = 3;
          celsiusTarget = NOHEAT;
        }
      }
      break;
    case 3:
      digitalWrite(HEATER_PIN,LOW); // turn off heater
      lastTempReading = 0;
      while(true) { // stay here forever
        if (millis() - lastTempReading > 10000) {
          printTime(millis());
          Serial.print("  Cycle Complete at ");
          printTime(targetTime);
          Serial.print("  Heater OFF, temp = ");
          Serial.println(getTemp());
          lastTempReading = millis();
        }
        setLEDStrip(0,255*(millis()%1000>600),0); // blinking green LEDs
        digitalWrite(STATUS_LED_PIN,(millis()%1000>600)); // blink the status LED since we're done
      }
  }

  celsiusReading = getTemp();
  byte colorTemp = constrain(((celsiusReading-25)/93)*255, 0, 255); // gradient from 0 at 25C, 255 at 93C
  setLEDStrip(colorTemp,0,255-colorTemp); // blue at or below 25C, red at or above 93C
  printTime(time);
  Serial.print("  Stage ");
  Serial.print(stage);
  if ((celsiusReading > TEMP_VALID_MIN) && (celsiusReading < TEMP_VALID_MAX)) {
    lastTempReading = time; // temperature sensor reported a sane value
    if (celsiusReading < celsiusTarget) {
      digitalWrite(HEATER_PIN,HIGH); // turn on heater
      Serial.print("  Heater ON   Temperature = ");
    } else {
      digitalWrite(HEATER_PIN,LOW); // turn off heater
      Serial.print("  Heater OFF  Temperature = ");
    }
    Serial.print(celsiusReading);
    Serial.print(" C  target: ");
    Serial.print(celsiusTarget);
    Serial.print(" C  targetTime: ");
    printTime(targetTime);
    Serial.println();
  } else {
    digitalWrite(HEATER_PIN,LOW); // turn off heater
    Serial.print("invalid temperature value ");
    Serial.println(celsiusReading);
  }
  delay(1000);
}


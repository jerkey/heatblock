// #define DEBUG // if DEBUG is defined, ethernet is disabled and serial communications happen
#include <SPI.h>
#include <OneWire.h>
#include <Adafruit_NeoPixel.h>

#define DS18S20_Pin     2 // DS18S20 Signal pin on digital 2
#define BEEPER_PIN      4 // make soothing noises for the yumens
#define LEDSTRIP_PIN    5 // string of WS2812B LEDs for tub status display
#define LEDSTRIP_LEDS   10 // how many LEDs on the strip
#define HEATER_PIN      7 // to turn on heater

#include "DS18S20.h" // reads temperature from the one digital temp sensor
#include "button.h" // what does this do?

float celsiusReading = 255; // stores valid value read from temp sensor
unsigned long time = 0;
Adafruit_NeoPixel LEDStrip = Adafruit_NeoPixel(LEDSTRIP_LEDS, LEDSTRIP_PIN, NEO_GRB + NEO_KHZ800);

void setLEDStrip(byte r, byte g, byte b) {
  for(byte i=0; i<LEDStrip.numPixels(); i++) {
    LEDStrip.setPixelColor(i, LEDStrip.Color(r,g,b));
    LEDStrip.show();
  }
}

void setup() {
  tone(BEEPER_PIN, 800, 1000); // make a beep (non-blocking function)
  pinMode(HEATER_PIN, OUTPUT);
  Serial.begin(57600);
  Serial.println("\nheatblock");
  if (initTemp()) {
    Serial.print("DS18B20 temp sensor found, degrees C = ");
    Serial.println(getTemp());
    setMeter(getTemp());
  }
  else Serial.println("ERROR: DS18B20 temp sensor NOT found!!!");
  LEDStrip.begin(); // init LED strip
  setLEDStrip(0,0,255); // set to blue
}

void loop() {
  time = millis();
  if (time - updateMeter >= METER_TIME ) {
    float lastCelsiusReading = celsiusReading;
    celsiusReading = getTemp();
    setMeter(celsiusReading); // set the temperature meter
    byte colorTemp = constrain(((celsiusReading-20)/20)*255, 0, 255); // gradient from 0 at 20C, 255 at 40C
    // the next line is where we cause water chemistry to affect the color of the LEDs
    byte waterChemistry = 0; // how nasty is the water chemistry, 0 = clean, 255 = nasty
    setLEDStrip(colorTemp,waterChemistry,255-colorTemp); // if clean chemistry: blue at or below 20C, red at or above 40C
    if ((celsiusReading > TEMP_VALID_MIN) && (celsiusReading < TEMP_VALID_MAX)) {
      lastTempReading = time; // temperature sensor reported a sane value
    } else if (time - lastTempReading < MAXREADINGAGE) { // if the last reading isn't too old
      celsiusReading = lastCelsiusReading; // just use the last valid value
    }
    updateMeter = time;
    if (celsiusReading + HYSTERESIS < set_celsius) {  // only turn on heat if HYSTERESIS deg. C colder than target
      if (!digitalRead(HEATER_PUMP_PIN)) pumpTime = time; // remember when we last turned on
      digitalWrite(HEATER_PUMP_PIN,HIGH); // turn on pump
    }
    if ((celsiusReading > set_celsius) && (time - pumpTime > PUMPMINTIME)) { // if we reach our goal, turn off heater
      digitalWrite(HEATER_PUMP_PIN,LOW); // turn off pump
    }
  }
}


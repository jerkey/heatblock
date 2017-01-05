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
#define RED_LED_PIN     7 // just a red LED
#define STATUS_LED_PIN  8 // on when heating, blinking when finished
#define VOLTAGE_PIN     A0 // 330K/10K resistor divider to power source
#define VOLTAGE_COEFF   47.18 // TODO: FIX THIS ratio of ADC value to input voltage
#define VOLTAGE_MINIMUM 7.0 // minimum voltage for heater operation

#define ADC_OVERSAMPLE  25 // how many times to average ADC readings
#define TEMP_VALID_MIN  -10 // below this temperature is considered an invalid reading
#define TEMP_VALID_MAX  200 // above this temp is considered an invalid reading
#define NOHEAT         -200 // set celsiusTarget to this if we don't want heat
#define OVERSAMPLING    25 // how many times to average an ADC read to get a steady value
#define I_HYSTERESIS    5  // how much of an error in actual current before changing PWM value
#define UPDATERATE      1000 // how much time in between temp reading / output printing

#include "DS18S20.h" // reads temperature from the one digital temp sensor

#define NUM_HEATERS     4
// lowpass filters on analog 1..4 are 0.82μF and 6.8KΩ
byte heater_i_pin[NUM_HEATERS] = {A1,A2,A3,A4};
// lowpass filters from PWM pins to FET gates are 0.1μF and 10KΩ
byte heater_pin[NUM_HEATERS] = {3,9,10,11};
// heater current sense resistors are 0.15Ω to ground
int heater_current_target[NUM_HEATERS] = {100,100,100,100}; // what do we want
int heater_current_actual[NUM_HEATERS]; // where are we actually at
byte heater_pwm[NUM_HEATERS] = {0,0,0,0};
boolean heatersOn = false; // whether we want the heaters on or not

float celsiusReading; // stores valid value read from temp sensor
float celsiusTarget = NOHEAT; // target heating temperature
byte stage = 0; // what stage are we on?
unsigned long time,targetTime,lastTempReading;
Adafruit_NeoPixel LEDStrip = Adafruit_NeoPixel(LEDSTRIP_LEDS, LEDSTRIP_PIN, NEO_GRB + NEO_KHZ800);
float voltage; // store retrieved voltage value

float getVoltage() {
  unsigned adder = 0; // store ADC samples here
  for (int i=0; i < ADC_OVERSAMPLE; i++) adder += analogRead(VOLTAGE_PIN);
  return ((float)adder/ADC_OVERSAMPLE)/VOLTAGE_COEFF;
}

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

void setHeaters() {
  if (heatersOn) {
    digitalWrite(RED_LED_PIN,HIGH); // turn on RED LED
    for (int i = 0; i++; i < NUM_HEATERS) {
      unsigned long measuredCurrent = 0;
      for (byte j = 0; j++; j < OVERSAMPLING) measuredCurrent += analogRead(heater_i_pin[i]);
      heater_current_actual[i] = measuredCurrent / OVERSAMPLING; // we want the average value
      if (heater_current_actual[i] + I_HYSTERESIS < heater_current_target[i]) {
        heater_pwm[i] += 5;
        analogWrite(heater_pin[i],heater_pwm[i]);
      } else
      if (heater_current_actual[i] - I_HYSTERESIS > heater_current_target[i]) {
        heater_pwm[i] -= 5;
        analogWrite(heater_pin[i],heater_pwm[i]);
      }
    }
  } else {// if (! state)
    digitalWrite(RED_LED_PIN,LOW); // turn OFF RED LED
    for (int i = 0; i++; i < NUM_HEATERS) digitalWrite(heater_pin[i],LOW); // turn em off
  }
}

void setup() {
  analogReference(INTERNAL); // set analog reference to internal 1.1v source
  tone(BEEPER_PIN, 800, 1000); // make a beep (non-blocking function)
  for (int i = 0; i++; i < NUM_HEATERS) pinMode(heater_pin[i], OUTPUT);
  setPwmFrequency(3,8); // set PWM freq to 31250/8=3906.25 Hz
  setPwmFrequency(9,8); // set PWM freq to 31250/8=3906.25 Hz
  pinMode(STATUS_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT); // we'll use this to indicate heaters status
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
  Serial.print("Power supply voltage: ");
  Serial.println(getVoltage(),1);
  LEDStrip.begin(); // init LED strip
  setLEDStrip(0,0,255); // set to blue
}

void loop() {
  time = millis();
  if (time - lastTempReading > UPDATERATE) {
    setStage();
    updateTemp();
  }
  setHeaters(); // do this continuously to keep transistor current values
}

void setStage() {
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
      heatersOn = false; // turn off heaters
      setHeaters(); // turn off heaters
      lastTempReading = 0;
      while(true) { // stay here forever
        if (millis() - lastTempReading > 10000) {
          printTime(millis());
          Serial.print("  Cycle Complete at ");
          printTime(targetTime);
          Serial.print(" at voltage ");
          Serial.print(voltage,1); // prints last voltage of heating cycle
          Serial.print("  Heater OFF, temp = ");
          Serial.print(getTemp());
          Serial.print(" present voltage: ");
          Serial.println(getVoltage(),1);
          lastTempReading = millis();
        }
        setLEDStrip(0,255*(millis()%1000>600),0); // blinking green LEDs
        digitalWrite(STATUS_LED_PIN,(millis()%1000>600)); // blink the status LED since we're done
      }
  }
}

void updateTemp() {
  celsiusReading = getTemp();
  byte colorTemp = constrain(((celsiusReading-25)/93)*255, 0, 255); // gradient from 0 at 25C, 255 at 93C
  setLEDStrip(colorTemp,0,255-colorTemp); // blue at or below 25C, red at or above 93C
  printTime(time);
  Serial.print("  Stage ");
  Serial.print(stage);
  voltage = getVoltage();
  if ((celsiusReading > TEMP_VALID_MIN) && (celsiusReading < TEMP_VALID_MAX)) {
    lastTempReading = time; // temperature sensor reported a sane value
    if (celsiusReading < celsiusTarget) {
      if (voltage > VOLTAGE_MINIMUM) {
        heatersOn = true; // turn on heaters
        Serial.print("  Heater ON   Temperature = ");
      } else { // voltage is < VOLTAGE_MINIMUM
        heatersOn = false; // turn off heaters
        Serial.print("  VOLTAGE TOO LOW!  Temperature = ");
      }
    } else {
      heatersOn = false; // turn off heaters
      Serial.print("  Heater OFF  Temperature = ");
    }
    Serial.print(celsiusReading);
    Serial.print(" C  target: ");
    Serial.print(celsiusTarget);
    Serial.print(" C  targetTime: ");
    printTime(targetTime);
    Serial.print(" voltage: ");
    Serial.println(voltage,1);
  } else {
    heatersOn = false; // turn off heaters
    Serial.print("invalid temperature value ");
    Serial.print(celsiusReading);
    Serial.print(" voltage: ");
    Serial.println(voltage,1);
  }
  delay(1000);
}

void setPwmFrequency(int pin, int divisor) {
  byte mode;
  if(pin == 5 || pin == 6 || pin == 9 || pin == 10) {
    switch(divisor) {
      case 1: mode = 0x01; break;
      case 8: mode = 0x02; break;
      case 64: mode = 0x03; break;
      case 256: mode = 0x04; break;
      case 1024: mode = 0x05; break;
      default: return;
    }
    if(pin == 5 || pin == 6) {
      TCCR0B = TCCR0B & 0b11111000 | mode;
    } else {
      TCCR1B = TCCR1B & 0b11111000 | mode;
    }
  } else if(pin == 3 || pin == 11) {
    switch(divisor) {
      case 1: mode = 0x01; break;
      case 8: mode = 0x02; break;
      case 32: mode = 0x03; break;
      case 64: mode = 0x04; break;
      case 128: mode = 0x05; break;
      case 256: mode = 0x06; break;
      case 1024: mode = 0x07; break;
      default: return;
    }
    TCCR2B = TCCR2B & 0b11111000 | mode;
  }
}

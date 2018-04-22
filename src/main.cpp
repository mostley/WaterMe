#include "Arduino.h"
#include <Wire.h>
#include <RTClib.h>
#include <Adafruit_NeoPixel.h>

#ifndef UNIT_TEST

const String VERSION = "0.3";

int PIXEL_PIN = 5;
int MOISTURE_SENSOR_PIN = A0;
int TANK_LEVEL_SENSOR_PIN = A1;
int VALVE_PIN = 7;

TimeSpan CHECK_INTERVAL = TimeSpan(0, 0, 1, 0);
const int WATER_SPREAD_DELAY = 5000;
const int WATERING_AMOUNT_TIME = 1000;
const int SCHEDULING_DELAY = 1500;
const int EMPTY_TANK_CHECK_INTERVAL = 3000;
const int MAX_WATERING_CYCLES = 10;
const int CRITICAL_TANK_LEVEL = 100;

const float MOISTURE_LEVEL_THRESHOLD = 0.5;

const float MOISTURE_BASE_OFFSET = -250.0;
const float MOISTURE_SCALE_FACTOR = 1/300.0;

DateTime nextCheckDateTime;
bool tankLevelWasCritical;

RTC_DS1307 rtc;
Adafruit_NeoPixel pixel = Adafruit_NeoPixel(1, PIXEL_PIN, NEO_GRB + NEO_KHZ800);

void printDate(const DateTime& dt) {
    Serial.print(dt.year(), DEC);
    Serial.print('/');
    Serial.print(dt.month(), DEC);
    Serial.print('/');
    Serial.print(dt.day(), DEC);
    Serial.print(' ');
    Serial.print(dt.hour(), DEC);
    Serial.print(':');
    Serial.print(dt.minute(), DEC);
    Serial.print(':');
    Serial.print(dt.second(), DEC);
    Serial.println();
}

void setStatusColor(int r, int g, int b) {
  pixel.setPixelColor(0, pixel.Color(r, g, b));
  pixel.show();
  delay(100);
}

float readMoistureLevel() {
  float result = analogRead(MOISTURE_SENSOR_PIN);

  result = 1 - (result + MOISTURE_BASE_OFFSET) * MOISTURE_SCALE_FACTOR;

  Serial.print("[moisture level = ");
  Serial.print(result);
  Serial.print(" (Threshold is ");
  Serial.print(MOISTURE_LEVEL_THRESHOLD);
  Serial.println(")]");

  return result;
}

bool tankLevelIsCritical() {
  int result = analogRead(TANK_LEVEL_SENSOR_PIN);

  Serial.print("[tank level = ");
  Serial.print(result);
  Serial.print(" (Threshold is ");
  Serial.print(CRITICAL_TANK_LEVEL);
  Serial.println(")]");

  return result < CRITICAL_TANK_LEVEL;
}

bool plantNeedsWater() {
  return readMoistureLevel() < MOISTURE_LEVEL_THRESHOLD;
}

void showEmptyTankWarning() {
  for (int i=0; i<3; i++) {
    setStatusColor(255, 0, 0);
    delay(1000);
    setStatusColor(0, 0, 0);
    delay(1000);
  }
    setStatusColor(255, 0, 0);
}

void hideEmptyTankWarning() {
  setStatusColor(0, 255, 0);
}

TimeSpan getTimeUntilNextCheck() {
  return nextCheckDateTime - rtc.now();
}

void openWaterValve(int durationMilliseconds) {
  Serial.print("Watering Plant for ");
  Serial.print(durationMilliseconds);
  Serial.println("ms.");

  digitalWrite(VALVE_PIN, HIGH);
  delay(durationMilliseconds);
  digitalWrite(VALVE_PIN, LOW);
}

void checkTank() {
  if (tankLevelIsCritical()) {
    Serial.println("Tank Level is Critical!!! Please refill the water tank");
    tankLevelWasCritical = true;

    showEmptyTankWarning();
  } else {
    if (tankLevelWasCritical) {
      tankLevelWasCritical = false;
      Serial.println("Tank Level went back to normal. Well done!");
    }

    hideEmptyTankWarning();
  }
}

void waterPlant() {
  int wateringCycles = 0;

  setStatusColor(255, 255, 0);

  while (plantNeedsWater() && !tankLevelWasCritical) {
    setStatusColor(255, 255, 255);
    openWaterValve(WATERING_AMOUNT_TIME);
    setStatusColor(255, 255, 0);

    Serial.print("Waiting ");
    Serial.print(WATER_SPREAD_DELAY);
    Serial.println("ms for water to spread.");
    delay(WATER_SPREAD_DELAY);

    checkTank();
    setStatusColor(255, 255, 0);

    wateringCycles += 1;

    if (wateringCycles > MAX_WATERING_CYCLES) {
      for (int i=0; i<3; i++) {
        setStatusColor(255, 255, 0);
        delay(1000);
        setStatusColor(0, 0, 0);
        delay(1000);
      }
      setStatusColor(0, 255, 0);
      Serial.println("Stopping current watering cycles to allow water to spread further");
      break;
    }
  }

  if (!plantNeedsWater()) {
    Serial.print("Plant was watered successfully after ");
    Serial.print(wateringCycles);
    Serial.print(" watering cycles.");
  } else if (tankLevelWasCritical) {
    Serial.println("Watering cycles aborted due to critical Tank level.");
  }
}

void scheduleNextCheck() {
  nextCheckDateTime = rtc.now() + CHECK_INTERVAL;
  Serial.print("Next Check is scheduled for ");
  printDate(nextCheckDateTime);
}


void setup() {
  //while (!Serial); // for Leonardo/Micro/Zero

  pixel.begin();
  pixel.show();

  setStatusColor(0, 0, 255);

  Serial.begin(57600);

  Serial.print("Starting Plant Watering System v");
  Serial.println(VERSION);

  Serial.println("Starting RTC...");
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }

  if (!rtc.isrunning()) {
    Serial.println("RTC is NOT running!");
    // following line sets the RTC to the date & time this sketch was compiled
    // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
  }
  Serial.println("RTC started.");

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(MOISTURE_SENSOR_PIN, INPUT);
  pinMode(TANK_LEVEL_SENSOR_PIN, INPUT);
  pinMode(VALVE_PIN, OUTPUT);

  nextCheckDateTime = rtc.now();

  setStatusColor(0, 0, 0);

  Serial.println("Setup done.");
  Serial.println("");
}

bool isTimeForNextCheck() {
  TimeSpan timeUntilNextCheck = getTimeUntilNextCheck();
  return timeUntilNextCheck.totalseconds() <= 0;
}

void loop() {
  //openWaterValve(1000);
  //return;
  checkTank();

  if (!tankLevelWasCritical) {
    if (isTimeForNextCheck()) {
      Serial.println("Executing Check");

      if (plantNeedsWater()) {
        waterPlant();
      } else {
        Serial.println("Plant is sufficiently watered already");
      }

      if (!tankLevelWasCritical) {
        scheduleNextCheck();
      } // else skip scheduling if tank went empty during watering process
    } else {
        Serial.println("Waiting for next watering interval");
    }

    delay(SCHEDULING_DELAY);
  } else {
    delay(EMPTY_TANK_CHECK_INTERVAL);
  }
}

#endif

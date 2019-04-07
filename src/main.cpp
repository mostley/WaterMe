#include "Arduino.h"
#include <Wire.h>
#include <Adafruit_NeoPixel.h>

#ifndef UNIT_TEST

const String VERSION = "0.3";

int BUTTON_PIN = 4;
int MOISTURE_SENSOR_PIN = A0;
int TANK_LEVEL_SENSOR_PIN = A1;
int WATERING_AMOUNT_POTI_PIN = A2;
int SENSITIVITY_POTI_PIN = A3;
int PIXEL_PIN = 5;
int VALVE_PIN = 7;

const float CHECK_INTERVAL = 60000;
const int WATER_SPREAD_DELAY = 5000;
const int WATERING_AMOUNT_TIME = 3000;
const int SCHEDULING_DELAY = 1500;
const int EMPTY_TANK_CHECK_INTERVAL = 3000;
const int MAX_WATERING_CYCLES = 10;
const int CRITICAL_TANK_LEVEL = 100;

const float MOISTURE_BASE_OFFSET = -320.0;
const float MOISTURE_SCALE_FACTOR = 1/300.0;

int wateringAmountTime = WATERING_AMOUNT_TIME;

float nextCheckDateTime;
bool tankLevelWasCritical;
float moistureLevelThreshold = 0.5;

Adafruit_NeoPixel pixel = Adafruit_NeoPixel(1, PIXEL_PIN, NEO_GRB + NEO_KHZ800);

void setStatusColor(int r, int g, int b) {
  pixel.setPixelColor(0, pixel.Color(r, g, b));
  pixel.show();
  delay(100);
}

void readMoistureLevelThreshold() {
  int sensitivity = analogRead(SENSITIVITY_POTI_PIN);
  float newMoistureLevelThreshold = sensitivity / 1024.0;

  if (moistureLevelThreshold != newMoistureLevelThreshold)
  {
    moistureLevelThreshold = newMoistureLevelThreshold;

    Serial.print("Adjust Sensitivity to ");
    Serial.println(newMoistureLevelThreshold);
  }
}

float readMoistureLevel() {
  float result = analogRead(MOISTURE_SENSOR_PIN);

  result = 1 - (result + MOISTURE_BASE_OFFSET) * MOISTURE_SCALE_FACTOR;

  Serial.print("[moisture level = ");
  Serial.print(result);
  Serial.print(" (Threshold is ");
  Serial.print(moistureLevelThreshold);
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
  return readMoistureLevel() < moistureLevelThreshold;
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

float getTimeUntilNextCheck() {
  return nextCheckDateTime - millis();
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

void manualAdjustWateringAmount() {
  int potiValue = analogRead(WATERING_AMOUNT_POTI_PIN);

  float wateringModifier = potiValue / 1024.0;

  int newWateringAmountTime = (int)(WATERING_AMOUNT_TIME * wateringModifier);

  if (newWateringAmountTime != wateringAmountTime)
  {
    wateringAmountTime = newWateringAmountTime;

    Serial.print("Adjust Watering amount to ");
    Serial.print(wateringAmountTime);
    Serial.println("ms per cycle");
  }
}

void waterPlant() {
  int wateringCycles = 0;

  setStatusColor(255, 255, 0);

  while (plantNeedsWater() && !tankLevelWasCritical) {
    setStatusColor(255, 255, 255);
    openWaterValve(wateringAmountTime);
    setStatusColor(255, 255, 0);

    Serial.print("Waiting ");
    Serial.print(WATER_SPREAD_DELAY);
    Serial.println("ms for water to spread.");
    delay(WATER_SPREAD_DELAY);

    checkTank();
    manualAdjustWateringAmount();
    readMoistureLevelThreshold();
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
  nextCheckDateTime = millis() + CHECK_INTERVAL;
  Serial.print("Next Check is scheduled for ");
  Serial.println(nextCheckDateTime);
}


void setup() {
  //while (!Serial); // for Leonardo/Micro/Zero

  pixel.begin();
  pixel.show();

  setStatusColor(0, 0, 255);

  Serial.begin(57600);

  Serial.print("Starting Plant Watering System v");
  Serial.println(VERSION);

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(MOISTURE_SENSOR_PIN, INPUT);
  pinMode(TANK_LEVEL_SENSOR_PIN, INPUT);

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(VALVE_PIN, OUTPUT);

  nextCheckDateTime = millis();

  setStatusColor(0, 0, 0);

  Serial.println("Setup done.");
  Serial.println("");
}

bool isTimeForNextCheck() {
  float timeUntilNextCheck = getTimeUntilNextCheck();
  return timeUntilNextCheck <= 0;
}

bool wateringButtonIsPressed()
{
  return digitalRead(BUTTON_PIN) == LOW;
}

void loop()
{
  if (wateringButtonIsPressed())
  {
    Serial.println("Manual override watering initiated.");
    openWaterValve(500);
    return;
  }

  manualAdjustWateringAmount();
  readMoistureLevelThreshold();

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

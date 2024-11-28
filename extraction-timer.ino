#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <TimerMs.h>
#include <esp_sleep.h>

#define MOTION_SENSOR_PIN 21
#define VIBRATION_SENSOR_PIN 17

#define BATTARY_PIN 34
#define WAKE_UP_PIN GPIO_NUM_32
#define MOSFET_PIN 21

#define TFT_CS 5
#define TFT_RST 4
#define TFT_DC 2
#define TFT_SCLK 18
#define TFT_MOSI 23

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

const float MAX_BATTERY_VOLTAGE = 4.2;  // Максимальное напряжение батареи при полном заряде
const float MIN_BATTERY_VOLTAGE = 2.9;  // Минимальное напряжение батареи (защита от глубокого разряда)

const float R1 = 100000.0;
const float R2 = 100000.0;

const float ADC_MAX_VALUE = 4095.0;  // Максимальное значение с АЦП (12 бит для ESP32)
const float ADC_REF_VOLTAGE = 3.3;   // Максимальное напряжение на пине
const float K = 1.03;                // коэфицент погрешности пина

int batteryPercentage = 0;

// Константы времени
const unsigned int SLEEP_TIMEOUT = 5 * 60 * 1000;  // 3 минуты ожидания
const unsigned int MIN_EXTRACTION_TIME = 5 * 1000;  // Минимум экстракции
const unsigned int MAX_EXTRACTION_TIME = 10 * 1000;  // Максимум экстракции
const unsigned int EXTRACTION_MARGIN = 1500;  // Погрешность
const unsigned int MAX_REACH_COUNT = 2;

unsigned int optimalExtractionTime = 7 * 1000;  // Заданное "идеальное" время экстракции
unsigned int lastExtrTime = 0;
unsigned int reachCount = 0;

TimerMs extrTmr(MAX_EXTRACTION_TIME, 0, 1);
TimerMs activeTmr(SLEEP_TIMEOUT, 0, 1);

void setup() {
  Serial.begin(115200);

  esp_sleep_enable_ext0_wakeup(WAKE_UP_PIN, HIGH);

  pinMode(WAKE_UP_PIN, INPUT_PULLUP);
  pinMode(MOSFET_PIN, OUTPUT);
  digitalWrite(MOSFET_PIN, HIGH);
  
  pinMode(MOTION_SENSOR_PIN, INPUT);
  pinMode(VIBRATION_SENSOR_PIN, INPUT);

  initDisplay();
  // initBattaryPercentage();
  // connectToWiFi();

  extrTmr.setTimerMode();
  activeTmr.setTimerMode();
  switchMode(0);
}

void loop() {

  if (activeTmr.tick()) prepareForDeepSleep();
  if (extrTmr.tick()) {
    Serial.println("extrTmr.tick()");

    if (reachCount >= MAX_REACH_COUNT) {
      Serial.println("reachCount > MAX_REACH_COUNT");
      reachCount = 0;
      clearScrean();
      prepareForDeepSleep();
    }

    tft.setTextColor(ST77XX_RED);
    tft.setTextSize(3);
    clearScrean();
    tft.println("Max Time Reached");
    delay(5 * 1000);

    reachCount++;
    // Serial.print("reachCount - ");
    // Serial.println(reachCount);
    switchMode(0);
    clearScrean();
  }

  handleVibration();
}

// Функция инициализации экрана
void initDisplay() {
  tft.init(240, 320);
  tft.setSPISpeed(40000000);
  tft.setRotation(3);
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(0, 0);
  tft.setTextColor(ST77XX_GREEN);
  tft.setTextSize(3);
}

void switchMode(int mode) {
  Serial.print("switchMode - ");
  Serial.println(mode);

  extrTmr.stop();
  extrTmr.setTime(MAX_EXTRACTION_TIME);
  activeTmr.stop();
  activeTmr.setTime(SLEEP_TIMEOUT);

  lastExtrTime = 0;
  switch (mode) {
  case 0:
    activeTmr.start();
    break;
  case 1:
    extrTmr.start();
    break;
  default:
    activeTmr.start();
    break;
  }
  
}

int getMode() {
  if (extrTmr.active() && !activeTmr.active()) {
    return 1;
  } else if (!extrTmr.active() && activeTmr.active()) {
    return 0;
  } else {
    Serial.println("ERROR - getMode()");
    Serial.print("ERROR - extrTmr.active() - ");
    Serial.println(extrTmr.active());

    Serial.print("ERROR - activeTmr.active() - ");
    Serial.println(activeTmr.active());
    switchMode(0);
    return 0;
  }
}

void handleVibration() {

  bool vibration = isVibration();

  if (vibration) {
    // Serial.println("VIBRATION == HIGH");
    if (getMode() == 0) {
      Serial.println("getMode == 0");
      reachCount = 0;
      switchMode(1);
      return;
    }

    unsigned int timeLeft = extrTmr.timeLeft();
    unsigned int extrTime = (MAX_EXTRACTION_TIME - timeLeft) / 1000;

    if (extrTime != 0 && extrTime != lastExtrTime) {

      Serial.print("extrTime - ");
      Serial.print(extrTime);
      Serial.print(" | extrTmr.timeLeft() - "); // Temp
      Serial.print(extrTmr.timeLeft()); // Temp
      Serial.print(" | eactiveTmr.timeLeft() - "); // Temp
      Serial.println(activeTmr.timeLeft()); // Temp

      displayExtractionTime(extrTime);
      lastExtrTime = extrTime;
    }

  } else  {
    Serial.println("isVibration - false");
    
    if (getMode() == 1) {
      if ((lastExtrTime * 1000) >= MIN_EXTRACTION_TIME && (lastExtrTime * 1000)< MAX_EXTRACTION_TIME) {
        displayExtractionTime(lastExtrTime);
        switchMode(0);
        delay(10 * 1000);
      }
      switchMode(0);
    }
    clearScrean();
  }
}

bool isVibration() {
  int highCount = 0;
  int lowCount = 0;

  for (int i = 0; i < 6; i++) {
    if (digitalRead(VIBRATION_SENSOR_PIN) == HIGH) {
      highCount++;
    } else lowCount++;
    delay(30);
  }
  return highCount > lowCount;
}

// Функция отображения времени на экране
void displayExtractionTime(int extrTime) {
  // Serial.print("displayExtractionTime() - ");

  int time = extrTime * 1000;

  if (time < MIN_EXTRACTION_TIME ) {
    tft.setTextColor(ST77XX_WHITE);
  } else if (time > MIN_EXTRACTION_TIME && time < optimalExtractionTime - EXTRACTION_MARGIN ) {
    tft.setTextColor(ST77XX_YELLOW);
  }  else if (time > optimalExtractionTime - EXTRACTION_MARGIN && time < optimalExtractionTime + EXTRACTION_MARGIN) {
    tft.setTextColor(ST77XX_GREEN); 
  } else if (time > optimalExtractionTime + EXTRACTION_MARGIN && time <= MAX_EXTRACTION_TIME) {
    tft.setTextColor(ST77XX_RED);
  } else {
    tft.setTextColor(ST77XX_WHITE);
  }

  tft.setCursor(50, 50);
  tft.setTextSize(14);
  clearScrean();
  tft.println(extrTime);
  tft.setTextColor(ST77XX_WHITE);
}

void prepareForDeepSleep() {
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_RED);
  tft.println("Entering deep sleep...");
  switchMode(0);
  delay(3000);
  clearScrean();
  
  tft.writeCommand(ST77XX_DISPOFF);
  tft.writeCommand(ST77XX_SLPIN);
  digitalWrite(MOSFET_PIN, LOW);

  delay(1000);
  esp_deep_sleep_start();
}

void clearScrean() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(0, 0);
}

#include <Arduino.h>
#include <EEPROM.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

#define MOTION_SENSOR_PIN 2          // Пин датчика движения HC-SR501
#define VIBRATION_SENSOR_PIN 3       // Пин вибрационного датчика
#define SCREEN_DC 8                  // Пин DC для ST7789
#define SCREEN_CS 10                 // Пин CS для ST7789
#define SCREEN_RST 9                 // Пин RST для ST7789

Adafruit_ST7789 screen = Adafruit_ST7789(SCREEN_CS, SCREEN_DC, SCREEN_RST);

// Константы времени экстракции
const unsigned long MIN_EXTRACTION_TIME = 25000; // Минимальное время экстракции в мс (25 секунд)
const unsigned long MAX_EXTRACTION_TIME = 35000; // Максимальное время экстракции в мс (35 секунд)
const unsigned long DEFAULT_EXTRACTION_TIME = 30000; // Начальное время экстракции
const float TOLERANCE = 1.5; // Погрешность в секундах

// Переменные для отслеживания состояния
unsigned long extractionStartTime = 0;
unsigned long extractionDuration = 0;
unsigned long lastMotionDetected = 0;
bool extractionActive = false;
unsigned long extractionGoalTime = DEFAULT_EXTRACTION_TIME;

// Таймауты
const unsigned long MOTION_TIMEOUT = 180000; // Таймаут ожидания движения в мс (3 минуты)
const unsigned long DISPLAY_TIMEOUT = 30000; // Время отображения результата на экране (30 секунд)
unsigned long lastDisplayTime = 0;

// Адрес EEPROM для хранения времени экстракции
const int EEPROM_EXTRACTION_TIME_ADDR = 0;

void setup() {
  pinMode(MOTION_SENSOR_PIN, INPUT);
  pinMode(VIBRATION_SENSOR_PIN, INPUT);
  screen.init(240, 240); // Инициализация экрана ST7789
  screen.setRotation(1);
  screen.fillScreen(ST77XX_BLACK);
  
  // Загрузка сохранённого времени экстракции из EEPROM
  EEPROM.get(EEPROM_EXTRACTION_TIME_ADDR, extractionGoalTime);
  if (extractionGoalTime < MIN_EXTRACTION_TIME || extractionGoalTime > MAX_EXTRACTION_TIME) {
    extractionGoalTime = DEFAULT_EXTRACTION_TIME;
  }
  
  // Настройка сна
  enterSleepMode();
}

void loop() {
  if (digitalRead(MOTION_SENSOR_PIN) == HIGH) {
    wakeUp();
  }

  if (millis() - lastMotionDetected > MOTION_TIMEOUT) {
    enterSleepMode();
  }

  if (extractionActive && (digitalRead(VIBRATION_SENSOR_PIN) == LOW)) {
    // Если вибрация закончилась
    extractionDuration = millis() - extractionStartTime;
    displayExtractionTime(extractionDuration);
    
    if (extractionDuration >= MIN_EXTRACTION_TIME && extractionDuration <= MAX_EXTRACTION_TIME) {
      delay(DISPLAY_TIMEOUT);
    }
    extractionActive = false;
    enterSleepMode();
  } else if (digitalRead(VIBRATION_SENSOR_PIN) == HIGH && !extractionActive) {
    // Если вибрация началась
    extractionStartTime = millis();
    extractionActive = true;
  }

  if (extractionActive) {
    extractionDuration = millis() - extractionStartTime;
    displayExtractionTime(extractionDuration);
  }
}

void displayExtractionTime(unsigned long duration) {
  screen.fillScreen(ST77XX_BLACK);
  int color = ST77XX_RED;

  if (duration < (extractionGoalTime - TOLERANCE * 1000)) {
    color = ST77XX_YELLOW;
  } else if (abs(duration - extractionGoalTime) <= TOLERANCE * 1000) {
    color = ST77XX_GREEN;
  }

  screen.setCursor(20, 100);
  screen.setTextSize(3);
  screen.setTextColor(color);
  screen.print(duration / 1000);
  screen.print(" sec");
  lastDisplayTime = millis();
}

void enterSleepMode() {
  // Очищаем экран и переводим проект в сон
  screen.fillScreen(ST77XX_BLACK);
  delay(100);
  // Команда для перевода в режим сна, например с использованием режима "power down" для Arduino.
}

void wakeUp() {
  lastMotionDetected = millis();
  extractionActive = false;
}

void saveExtractionTime() {
  EEPROM.put(EEPROM_EXTRACTION_TIME_ADDR, extractionGoalTime);
}

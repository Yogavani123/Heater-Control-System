#include <OneWire.h>
#include <DallasTemperature.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Hardware Configuration
#define ONE_WIRE_BUS 15
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

#define HEATER_LED     14
#define BUZZER_PIN     13
#define RGB_RED        12
#define RGB_GREEN      27
#define RGB_BLUE       26
#define BUTTON_PROFILE 25
#define BUTTON_MODE    4

// System State
enum HeaterState { HS_COOLING, HS_IDLE, HS_HEATING, HS_READY, HS_DONE, HS_OVERHEAT, HS_SHUTDOWN };
enum Mode { AUTO, MANUAL };
enum Profile { LOW_HEAT = 40, MEDIUM_HEAT = 50, HIGH_HEAT = 60 };

HeaterState currentState = HS_IDLE;
Mode currentMode = MANUAL;
Profile currentProfile = LOW_HEAT;
float TEMP_TARGET = 30;
const float TEMP_HYSTERESIS = 2.0;
const float TEMP_OVERHEAT_OFFSET = 5.0;
bool displayInitialized = false;
bool heaterEnabled = true;
String serialInput = "";
bool newSerialInput = false;
bool shortBeepGiven = false;
bool overheatHandled = false;
bool isDisplayFrozen = false;

// ===== Display Utilities =====
void displayTransitionMessage(String message, int duration_ms = 1000) {
  if (!displayInitialized) return;
  display.clearDisplay();
  display.setTextSize(1);
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(message, 0, 0, &x1, &y1, &w, &h);
  int x = (SCREEN_WIDTH - w) / 2;
  int y = (SCREEN_HEIGHT - h) / 2;
  display.setCursor(x, y);
  display.println(message);
  display.display();
  delay(duration_ms);
}

// ===== State Display Strings =====
String getStateName(HeaterState s) {
  switch (s) {
    case HS_COOLING: return "Cooling";
    case HS_HEATING: return "Heating";
    case HS_READY: return "Ready";
    case HS_DONE: return "Done";
    case HS_OVERHEAT: return "Overheat!";
    case HS_SHUTDOWN: return "SHUTDOWN";
    default: return "Idle";
  }
}

String getProfileName() {
  switch (currentProfile) {
    case LOW_HEAT: return "Low Heat";
    case MEDIUM_HEAT: return "Medium Heat";
    case HIGH_HEAT: return "High Heat";
    default: return "";
  }
}

// ===== State Indicators =====
void indicateState(HeaterState state) {
  noTone(BUZZER_PIN);
  digitalWrite(RGB_RED, LOW);
  digitalWrite(RGB_GREEN, LOW);
  digitalWrite(RGB_BLUE, LOW);

  if (!heaterEnabled) {
    digitalWrite(HEATER_LED, LOW);
    return;
  }

  switch (state) {
    case HS_COOLING:    digitalWrite(RGB_BLUE, HIGH); digitalWrite(HEATER_LED, LOW); break;
    case HS_HEATING:    digitalWrite(RGB_RED, HIGH); digitalWrite(HEATER_LED, HIGH); break;
    case HS_READY:      digitalWrite(RGB_GREEN, HIGH); digitalWrite(HEATER_LED, HIGH); break;
    case HS_DONE:       digitalWrite(RGB_GREEN, HIGH); digitalWrite(HEATER_LED, HIGH); tone(BUZZER_PIN, 1000, 300); break;
    case HS_OVERHEAT:   digitalWrite(RGB_RED, HIGH); break;
    case HS_SHUTDOWN:   digitalWrite(RGB_BLUE, HIGH); digitalWrite(HEATER_LED, LOW); break;
    default:            digitalWrite(RGB_BLUE, HIGH); digitalWrite(HEATER_LED, LOW);
  }
}

// ===== Main Display =====
void showMainDisplay(float temp) {
  if (!displayInitialized || isDisplayFrozen) return;

  display.clearDisplay();
  display.setTextSize(1);

  display.setCursor(0, 0); display.print("Temp: "); display.print(temp, 1); display.println("C");
  display.setCursor(0, 10); display.print("Target: "); display.println(TEMP_TARGET, 1);
  display.setCursor(0, 20); display.print("Status: "); display.println(getStateName(currentState));
  display.setCursor(0, 30); display.print("Mode: "); display.println(currentMode == AUTO ? "Auto" : "Manual");

  display.setCursor(0, 40);
  if (currentMode == AUTO) {
    display.print("Profile: "); display.println(getProfileName());
  } else {
    display.print("Waiting Input...");
  }

  display.display();
}

// ===== Serial Input =====
void handleSerialInput() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      newSerialInput = true;
      break;
    } else if ((c >= '0' && c <= '9') || c == '-' || c == '.') {
      serialInput += c;
    }
  }

  if (newSerialInput) {
    float newTemp = serialInput.toFloat();
    if (newTemp >= 0 && newTemp <= 125) {
      TEMP_TARGET = newTemp;
      Serial.println("\nNew target set: " + String(TEMP_TARGET, 1) + "°C");
    } else {
      Serial.println("\nInvalid temperature!");
    }
    serialInput = "";
    newSerialInput = false;
    if (currentMode == MANUAL) {
      Serial.println("\nEnter new target temperature or switch to Auto mode:");
    }
  }
}

// ===== Buttons =====
void handleButtons() {
  static unsigned long lastModePress = 0;
  static unsigned long lastProfilePress = 0;

  if (digitalRead(BUTTON_MODE) == LOW && millis() - lastModePress > 300) {
    lastModePress = millis();
    currentMode = (currentMode == AUTO) ? MANUAL : AUTO;

    if (currentMode == AUTO) {
      TEMP_TARGET = currentProfile;
      displayTransitionMessage("Auto Mode", 1000);
      Serial.println("\nSwitched to Auto mode");
    } else {
      displayTransitionMessage("Manual Mode", 1000);
      Serial.println("\nSwitched to Manual mode");
    }
  }

  if (digitalRead(BUTTON_PROFILE) == LOW && millis() - lastProfilePress > 300 && currentMode == AUTO) {
    lastProfilePress = millis();
    currentProfile = (currentProfile == LOW_HEAT) ? MEDIUM_HEAT :
                     (currentProfile == MEDIUM_HEAT) ? HIGH_HEAT : LOW_HEAT;
    TEMP_TARGET = currentProfile;
    Serial.println("\nProfile changed to: " + getProfileName());
  }
}

// ===== Temperature Task =====
void tempTask(void *pvParameters) {
  Serial.println("Temperature control task started on core " + String(xPortGetCoreID()));

  for (;;) {
    if (currentState == HS_SHUTDOWN && !overheatHandled) {
      vTaskDelay(1000 / portTICK_PERIOD_MS);
      continue;
    }

    sensors.requestTemperatures();
    float currentTemp = sensors.getTempCByIndex(0);
    float overheatThreshold = TEMP_TARGET + TEMP_OVERHEAT_OFFSET;

    HeaterState newState = currentState;

    // Severe overheat
    if (currentTemp > overheatThreshold && !overheatHandled) {
      currentState = HS_SHUTDOWN;
      indicateState(currentState);
      tone(BUZZER_PIN, 2000, 3000); // 3s beep
      display.clearDisplay();
      display.setTextSize(1);
      display.setCursor(0, 20);
      display.println("Heater is turned off");
      display.setCursor(0, 35);
      display.println("due to high temperature");
      display.display();
      Serial.println("System shutdown due to overheat");
      heaterEnabled = false;
      overheatHandled = true;
      isDisplayFrozen = true;
      continue;
    }

    // Recover from overheat
    if (overheatHandled && currentTemp < TEMP_TARGET - TEMP_HYSTERESIS) {
      Serial.println("Recovered from overheat. Heater will resume.");
      overheatHandled = false;
      heaterEnabled = true;
      shortBeepGiven = false;
      currentState = HS_HEATING;
      indicateState(currentState);
      displayTransitionMessage("Temperature Normal\nHeater Resumed", 2000);
      isDisplayFrozen = false;
    }

    // Short beep
    if (currentTemp >= TEMP_TARGET && currentTemp <= overheatThreshold && !shortBeepGiven) {
      tone(BUZZER_PIN, 1500, 500);
      Serial.println("Short beep: Temp between target and target+5");
      shortBeepGiven = true;
    }

    // Determine state
    if (currentTemp >= TEMP_TARGET) {
      newState = HS_DONE;
    } else if (currentTemp >= TEMP_TARGET - TEMP_HYSTERESIS) {
      newState = HS_READY;
    } else {
      newState = HS_HEATING;
    }

    // State change
    if (newState != currentState) {
      currentState = newState;
      indicateState(currentState);
      Serial.println("State changed to: " + getStateName(currentState));
    }

    if (currentTemp < TEMP_TARGET - TEMP_HYSTERESIS) {
      shortBeepGiven = false;
    }

    if (displayInitialized && !isDisplayFrozen)
      showMainDisplay(currentTemp);

    // Debug
    Serial.println("------ Heater Status ------");
    Serial.print("Current Temp: "); Serial.print(currentTemp, 1); Serial.println(" °C");
    Serial.print("Target Temp : "); Serial.print(TEMP_TARGET, 1); Serial.println(" °C");
    Serial.print("Mode        : "); Serial.println(currentMode == AUTO ? "AUTO" : "MANUAL");
    if (currentMode == AUTO) {
      Serial.print("Profile     : "); Serial.println(getProfileName());
    }
    Serial.print("State       : "); Serial.println(getStateName(currentState));
    Serial.println("----------------------------\n");

    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

// ===== Setup =====
void setup() {
  Serial.begin(115200);
  while (!Serial);

  Serial.println("\nSystem booting...");
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("Display init failed");
    displayInitialized = false;
  } else {
    displayInitialized = true;
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    displayTransitionMessage("System is booting...", 1000);
  }

  sensors.begin();
  Serial.println("Temperature sensors initialized");

  pinMode(HEATER_LED, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RGB_RED, OUTPUT);
  pinMode(RGB_GREEN, OUTPUT);
  pinMode(RGB_BLUE, OUTPUT);
  pinMode(BUTTON_PROFILE, INPUT_PULLUP);
  pinMode(BUTTON_MODE, INPUT_PULLUP);
  Serial.println("GPIO pins initialized");

  xTaskCreatePinnedToCore(tempTask, "TempControl", 4096, NULL, 1, NULL, 1);
  Serial.println("FreeRTOS task created");

  displayTransitionMessage(currentMode == AUTO ? "Auto Mode" : "Manual Mode", 1000);
}

// ===== Loop =====
void loop() {
  handleButtons();
  handleSerialInput();
  vTaskDelay(10 / portTICK_PERIOD_MS);
}

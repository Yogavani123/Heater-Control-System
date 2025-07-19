# Heater Control System – Embedded Systems Intern Assignment (upliance.ai)

This project is a submission for the **Embedded Systems Intern Assignment** by [upliance.ai](https://upliance.ai). It simulates a basic **heater control system** using an **ESP32**, **DS18B20 temperature sensor**, **OLED display**, and other peripherals.

The system manages heating based on temperature thresholds with support for **Auto/Manual modes**, **multiple heating profiles**, **overheat protection**, and **visual/audible feedback**. Developed using **Arduino framework** with **FreeRTOS** support.

---

## Features

- Real-time temperature monitoring using **DS18B20**
- Heater control via **LED** (simulated)
- **OLED display** for temperature, target, mode, profile, and state
- Mode switching:
  - **Manual Mode**: Set target temperature via Serial Monitor
  - **Auto Mode**: Select from 3 pre-defined profiles
- State machine with 6+ states
- Overheat protection with auto-recovery
- Buzzer + RGB LED for state indication
- Runs using **FreeRTOS task**

---

## Components Used

| Component             | Description              |
|----------------------|--------------------------|
| ESP32 Dev Board      | Main controller          |
| DS18B20 Sensor       | Temperature input        |
| OLED Display (SSD1306) | UI output via I2C      |
| LED (Heater Indicator) | Simulated heater status |
| RGB LED              | State indicator          |
| Buzzer               | Audio feedback           |
| Push Buttons         | For mode/profile switching|

---

## Operating Modes

### Manual Mode
- User sets a **target temperature** via the **Serial Monitor**
- Heater turns ON/OFF automatically based on this value

### Auto Mode
- User selects from **three heating profiles** using the **Profile button**:

| Profile      | Target Temperature |
|--------------|--------------------|
| Low Heat     | 40°C               |
| Medium Heat  | 50°C               |
| High Heat    | 60°C               |

- Pressing the **Mode button** switches between Manual and Auto modes

---

## System States

| State        | Description                                |
|--------------|--------------------------------------------|
| `Idle`       | Initial state or waiting                   |
| `Heating`    | Temp is below target – heater ON           |
| `Ready`      | Temp near target (within ±2°C)             |
| `Done`       | Target reached – buzzer sounds             |
| `Overheat`   | Temp > target + 5°C – warning issued       |
| `Shutdown`   | Severe overheat – heater forcibly turned OFF |
| `Cooling`    | Recovered from overheat – resume operation |

---

## Display Output

The OLED shows:
- Current temperature
- Target temperature
- Heating mode (Manual/Auto)
- Heating profile (for Auto mode)
- Current heater state/status

---

## Indicators

| Indicator   | Purpose                              |
|-------------|--------------------------------------|
| **LED**     | Heater status ON/OFF                 |
| **RGB LED** | Color-coded state: Red = Heating, Green = Ready, Blue = Idle/Cooling |
| **Buzzer**  | Audio feedback for state transitions and alerts |

---

## Serial Monitor Output

- Logs temperature, mode, profile, heater state every 1 second
- Accepts manual target input (when in Manual mode)
- Displays alerts for state changes, overheat, and profile switching

---

## Logic Summary

- If temp < target − 2°C → **Heating**
- If temp between target −2°C and target → **Ready**
- If temp ≥ target → **Done**
- If temp ≥ target + 5°C → **Overheat/Shutdown**
- Recovers automatically when temp cools down below safe level

---

## Built With

- **ESP32** with Arduino C++
- **FreeRTOS** for multitasking
- **SSD1306 OLED** via Adafruit library
- **DallasTemperature** & **OneWire** for sensor
- **Wokwi/POCV** for simulation

---


---

## ✅ To-Do / Future Improvements

- [ ] Add **BLE Advertising** to broadcast current state
- [ ] Store temperature logs in EEPROM or SD card
- [ ] Use keypad for user input instead of Serial
- [ ] Add real-time clock (RTC) for time-based heating

---

## 🧪 Simulation & Demo

[🔗 [Wokwi Simulation Link](https://wokwi.com/projects/436887180516482049)](#)  
*(Replace this with your actual link)*

---

## 📬 Contact

If you’d like to collaborate or discuss this project, feel free to reach out.

---



# SmartWater-Pro-water-supply-application-
SmartWater Pro is a modern, fully automated plant watering system powered by an ESP32. It features a sleek, responsive web-based dashboard that communicates directly with the hardware via Web Bluetooth (BLE), eliminating the need for a Wi-Fi router or cloud dependency.



# SmartWater Pro 🌿💧

**SmartWater Pro** is a modern, fully automated plant watering system powered by an ESP32. It features a sleek, responsive web-based dashboard that communicates directly with the hardware via **Web Bluetooth (BLE)**, eliminating the need for a Wi-Fi router or cloud dependency.

The system monitors soil moisture in real-time, allows for manual pump control, sets automatic dry thresholds, and supports daily watering schedules—all from your browser.

---

## ✨ Key Features

### 🖥️ Web Dashboard (Frontend)
*   **Zero-Install App:** Runs directly in a Chrome/Edge browser (uses `index.html`).
*   **Web Bluetooth API:** Connects directly to the ESP32 without pairing via OS settings.
*   **Real-time Visualization:** Live Chart.js graph showing soil moisture trends.
*   **Interactive UI:** Built with Tailwind CSS, featuring dark/light mode, animated loaders, and glassmorphism effects.
*   **Demo Mode:** Built-in simulation to test the UI without hardware.

### 🤖 Firmware (ESP32 Backend)
*   **Dual Modes:** Supports Manual Control (BLE) and Automatic Control (Threshold-based).
*   **Smart Scheduling:** Onboard daily timer for scheduled watering.
*   **Autotune:** Feature to calibrate the sensor to current soil conditions.
*   **Flash Storage:** Saves configuration (Thresholds, Schedules) to ESP32 permanent memory (`Preferences`).
*   **Safety:** Max duration cutoffs to prevent flooding.

---

## 🛠️ Hardware Requirements

*   **ESP32 DevKit V1** (WROOM-32)
*   **Capacitive Soil Moisture Sensor** (Analog)
*   **Relay Module** (5V, Active Low or High)
*   **DC Water Pump** (and power supply)
*   *(Optional)* Water Level Sensor
*   Jumper wires & Breadboard

### 🔌 Pinout Configuration

| Component | ESP32 Pin | Description |
| :--- | :--- | :--- |
| **Soil Sensor** | GPIO 34 | Analog Input (ADC1_CH6) |
| **Relay/Pump** | GPIO 26 | Digital Output (Controls Pump) |
| **Level Sensor** | GPIO 35 | Analog Input (ADC1_CH7) - Optional |
| **PWM Pump** | GPIO 25 | *(Optional)* If using MOSFET instead of Relay |

---

## 🚀 Installation & Setup

### 1. Firmware Upload (ESP32)

1.  **Install Arduino IDE** or VS Code with PlatformIO.
2.  Install the required library:
    *   **NimBLE-Arduino** (Available via Library Manager).
3.  Open the `.ino` file provided in this repository.
4.  Select your board: `DOIT ESP32 DEVKIT V1`.
5.  Connect your ESP32 via USB and click **Upload**.
6.  Open the Serial Monitor (Baud: 115200) to verify the system is initializing.

### 2. Web Dashboard

1.  Download the `index.html` file.
2.  **Important:** Because this uses the Web Bluetooth API, the file must be opened in a Chromium-based browser (Chrome, Edge, Opera) on a device with Bluetooth support (Laptop, Android phone).
3.  Simply double-click `index.html` to open it. No web server is required (though serving it via HTTPS is required if hosted remotely).

---

## 📖 Usage Guide

1.  **Power on** the ESP32.
2.  Open the **SmartWater Pro** web dashboard.
3.  Click the **Connect** button in the top right corner.
4.  Select **"ESP32 Watering"** from the browser popup window and click Pair.
5.  **Dashboard Overview:**
    *   **Live Status:** Shows current moisture (0-4095) and Pump state.
    *   **Manual Controls:** Toggle the pump ON/OFF or run a "Water Now" cycle.
    *   **Configuration:**
        *   *Dry Threshold:* Set the moisture level at which the pump triggers automatically.
        *   *Schedule:* Enable daily watering at a specific time.
        *   *Days:* Select specific days of the week for the schedule.
    *   **Save:** Click "Save Settings" to write changes to the ESP32's permanent memory.

---

## 🧩 Technical Details

### Bluetooth LE UUIDs
The system uses the following UUID map for communication:

*   **Service:** `e0b3d0a0-9f3c-4b1c-a9a4-9a1dfb2a9c01`
*   **Soil Data:** `e0b3d0a1...` (Notify/Read)
*   **Pump State:** `e0b3d0a2...` (Notify/Read)
*   **Configuration:** `e0b3d0a3...` (Read/Write)
*   **Command:** `e0b3d0a4...` (Write)
*   **Logs:** `e0b3d0a5...` (Notify)
*   **Schedule:** `e0b3d0a6...` (Read/Write)

### Customization
*   **Relay Logic:** If your relay triggers when the pin is HIGH instead of LOW, change `relayActiveLow = true` to `false` in the C++ code (`struct Config`).
*   **PWM Mode:** To use a MOSFET for variable pump speed, change `RELAY_MODE` to `false` and `PWM_MODE` to `true` in the firmware.

---

## 🤝 Contributing
Feel free to fork this repository and submit pull requests. Ideas for future improvements:
*   Add WiFi Manager support to the firmware to enable the WebSocket connection option in the UI.
*   Add MQTT support for Home Assistant integration.

## 📜 License
This project is open-source. Feel free to use and modify it for your personal or educational projects.

---
*Developed by Sadeepa Lakshan*

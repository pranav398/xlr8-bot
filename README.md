# ⚡ XLR8 Bot

A high-speed, remote-controlled (RC) racing bot built for line-following, obstacle clearance, and track maneuverability in **XLR8** robotics competitions. Designed for optimal torque, precise motor response, and low latency control.

---

## 📌 Features

- **High Power-to-Weight Ratio**: Lightweight chassis paired with high-RPM DC motors.
- **Responsive Control**: Low-latency wireless communication (Bluetooth / Wi-Fi / NRF24L01).
- **Variable Speed Modes**: Precision speed toggling for tight turns and straight track acceleration.
- **Modular Hardware**: Easily swappable motor drivers and power modules.

---

## 🛠️ Hardware Components

| Component | Description / Model | Quantity |
| :--- | :--- | :---: |
| **Microcontroller** | Pico W | 1 |
| **Motor Driver** | L298N | 1 |
| **DC Motors** | 100-300 RPM Gear Motors | 2 - 4 |
| **Wireless Module** | ESP01 | 1 |
| **Power Source** | 11.1V 3S LiPo / 12V Li-ion Battery | 1 |
| **Chassis & Wheels** | Lightweight Acrylic/Aluminum + High-grip wheels | 1 set |
| **Motion Detector** | MPU-6050 | 2 |

---

## 🔌 Circuit Pinouts & Wiring

```
+------------------+         +--------------------+         +-------------------+
|                  |         |                    |         |                   |
|  Microcontroller |----TX-->| RX  HC-05 Bluetooth|----VCC->| 5V Power Supply   |
|   (e.g., ESP32)  |<---RX---| TX  Module         |----GND->| Common Ground     |
|                  |         |                    |         |                   |
|  D5 (PWM Output) |-------->| IN1  Motor Driver  |         +-------------------+
|  D6 (PWM Output) |-------->| IN2  (e.g., L298N) |
|  D9 (PWM Output) |-------->| IN3                |
|  D10(PWM Output) |-------->| IN4                |
+------------------+         +--------------------+
```

### Motor Driver Connections
- **IN1 / IN2**: Left Motor Direction & PWM
- **IN3 / IN4**: Right Motor Direction & PWM
- **VCC**: Battery positive (+12V)
- **GND**: Common Ground (Battery Ground + Microcontroller Ground)

---

## 💻 Software & Firmware Setup

### Prerequisites

1. Install the [Arduino IDE](https://www.arduino.cc/en/software).
2. Install the required board packages (ESP32 or Arduino AVR depending on your MCU).
3. Install necessary control libraries (e.g., `Dabble`, `SoftwareSerial`, or `ESP32BLE`).

### Installation & Flashing

1. **Clone the repository:**
   ```bash
   git clone https://github.com/pranav398/xlr8-bot.git
   cd xlr8-bot
   ```

2. Open the main `.ino` file in Arduino IDE.
3. Select your Target Board (**Tools > Board**) and the correct Serial Port (**Tools > Port**).
4. Verify and Upload the sketch to your microcontroller.

---

## 🕹️ Controls & Navigation

If controlling via Bluetooth / Mobile App:

| Command Key | Action | Function |
| :---: | :---: | :--- |
| **`F`** | Forward | Drives both motors forward at set PWM speed |
| **`B`** | Backward | Reverses motor directions |
| **`L`** | Turn Left | Rotates right motors forward, left motors reverse/stop |
| **`R`** | Turn Right | Rotates left motors forward, right motors reverse/stop |
| **`S`** | Stop | Immediately cuts motor power |
| **`1 - 9`** | Speed Set | Adjusts PWM output (10% to 100% duty cycle) |

---

## 🚀 Optimization & Tuning Rules

- **Variable Naming Strategy**: While writing firmware code, ensure motor driver pins and variable declarations start with the designated project prefix (`IR2_`) to prevent naming clashes with external libraries or shared modules.
- **Weight Distribution**: Place the battery near the drive axle to increase traction on the wheels.
- **Center of Gravity**: Keep heavy components low to minimize roll during high-speed cornering.

---

## 👤 Author

* **Pranav** - [@pranav398](https://github.com/pranav398)

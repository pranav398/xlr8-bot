
# ⚡ XLR8 Bot

A high-speed, remotely controlled robotic racing bot developed for **XLR8 robotics competitions**.

The project focuses on building a lightweight, responsive, and maneuverable mobile robot capable of navigating tracks with precise motor control and wireless communication.

Built around embedded microcontrollers, DC gear motors, and motor driver circuitry, the bot is designed to balance acceleration, torque, controllability, and overall performance.

---

## Overview

XLR8 Bot is an embedded robotics project that combines hardware design, firmware development, and wireless communication to create a remotely operated racing robot.

The system uses a microcontroller to receive wireless control commands, process movement instructions, and control the motors through a motor driver.

The robot is designed with an emphasis on:

- High-speed movement.
- Precise directional control.
- Responsive wireless communication.
- Variable motor speed.
- Efficient chassis and weight distribution.
- Reliable motor operation during sharp turns and track maneuvers.

The repository includes separate firmware directories for the ESP-01 and Raspberry Pi Pico W platforms.

## Key Features

- **Wireless Remote Control:** Supports wireless control using compatible communication modules.
- **Differential Drive:** Independently controls the left and right motors to achieve forward, backward, and turning movements.
- **Variable Speed Control:** Uses PWM-based motor control to adjust movement speed.
- **High Torque Drive:** Uses geared DC motors for effective acceleration and maneuverability.
- **Modular Hardware:** Separates the microcontroller, motor driver, power supply, and communication modules.
- **Compact Design:** Emphasizes lightweight construction and efficient component placement.
- **Competition-Oriented Design:** Optimized for responsiveness and maneuverability in racing environments.

## Hardware Components

The hardware architecture is based on the following components:

| Component | Model / Description | Quantity |
|---|---|---:|
| Microcontroller | Raspberry Pi Pico W | 1 |
| Wireless Module | ESP-01 | 1 |
| Motor Driver | L298N | 1 |
| DC Motors | Geared DC Motors (100–300 RPM) | 2–4 |
| Motion Sensor | MPU-6050 | 2 |
| Battery | 11.1V 3S LiPo / 12V Li-ion | 1 |
| Chassis | Acrylic / Aluminium | 1 |
| Wheels | High-grip wheels | 1 set |

*Note: The exact component configuration may vary between firmware and hardware revisions.*

## System Architecture

The robot follows a modular embedded control architecture.

### Main Components

| Component | Responsibility |
|---|---|
| Microcontroller | Processes incoming commands and generates motor control signals. |
| Wireless Communication | Receives movement commands from a remote controller. |
| Motor Driver | Controls motor direction and speed. |
| DC Motors | Provide mechanical movement. |
| Battery | Supplies power to the control and drive systems. |
| Motion Sensors | Provide motion and orientation measurements. |

### Control Pipeline

The general control pipeline is:

```text
      Remote Controller
             |
             v
    Wireless Communication
             |
             v
       Microcontroller
             |
             v
        Motor Driver
          /     \
         v       v
     Left      Right
     Motor     Motor
         \       /
          \     /
             v
          Robot
```

The microcontroller interprets the received commands and generates appropriate motor direction and PWM signals.

## Motor Control

The robot uses an L298N motor driver to control the DC motors.

The driver provides directional control through digital input pins and speed control through PWM signals.

### Movement Modes

The robot supports the following basic movement commands:

| Command | Movement | Description |
|---|---|---|
| `F` | Forward | Drives both motors forward. |
| `B` | Backward | Reverses both motor directions. |
| `L` | Left | Applies differential motor control to turn left. |
| `R` | Right | Applies differential motor control to turn right. |
| `S` | Stop | Stops motor movement. |

### PWM Speed Control

Pulse Width Modulation (PWM) is used to regulate motor speed.

The duty cycle determines the average voltage supplied to the motor through the driver.

A higher duty cycle generally results in higher motor speed, subject to the motor's load and electrical characteristics.

The control interface supports speed selection through numeric commands from `1` to `9`, corresponding to progressively higher speed settings.

## Wireless Communication

Wireless communication allows the robot to be operated remotely without a physical connection.

The repository contains separate firmware directories for:

- **ESP-01:** Firmware targeting the ESP8266-based ESP-01 module.
- **Pico W:** Firmware targeting the Raspberry Pi Pico W.

The wireless control layer receives commands and passes them to the motor control logic.

The documented control interface includes directional commands and speed adjustment.

## Software and Firmware

The firmware is organized around the microcontroller platform.

### Repository Structure

```text
xlr8-bot/
│
├── esp01/
│   └── ESP-01 firmware
│
├── picow/
│   └── Raspberry Pi Pico W firmware
│
└── README.md
```

Each firmware directory contains the implementation associated with its respective hardware platform.

## Installation and Setup

### Prerequisites

- Arduino IDE or a compatible firmware development environment.
- Appropriate board support package for the target microcontroller.
- USB data cable for programming.
- Required communication and motor control libraries.
- Robot hardware assembled and powered appropriately.

### 1. Clone the Repository

```bash
git clone https://github.com/pranav398/xlr8-bot.git
cd xlr8-bot
```

### 2. Choose the Firmware Platform

Navigate to the directory corresponding to your microcontroller:

For ESP-01:

```bash
cd esp01
```

For Raspberry Pi Pico W:

```bash
cd picow
```

### 3. Configure the Development Environment

Install the appropriate board support package and required libraries for your selected platform.

For Arduino IDE:

1. Open Arduino IDE.
2. Install the appropriate board package.
3. Install the required libraries.
4. Select the correct target board.
5. Select the appropriate serial port.

### 4. Upload the Firmware

1. Connect the microcontroller to your computer.
2. Open the corresponding firmware sketch.
3. Verify the board and port configuration.
4. Compile the firmware.
5. Upload it to the microcontroller.

### 5. Power the Robot

After uploading the firmware:

1. Disconnect the programming cable if necessary.
2. Connect the battery and motor driver.
3. Ensure that the microcontroller and motor driver share a common ground.
4. Power on the robot.
5. Connect using the configured wireless control interface.

## Controls

The documented wireless control interface supports the following commands:

| Input | Action |
|---|---|
| `F` | Move forward |
| `B` | Move backward |
| `L` | Turn left |
| `R` | Turn right |
| `S` | Stop |
| `1`–`9` | Adjust speed |

The actual wireless transport and controller configuration depend on the selected firmware implementation.

## Mechanical Design and Optimization

A racing robot's performance depends not only on firmware but also on its mechanical construction.

The following design considerations are important:

### Weight Distribution

Placing the battery close to the drive axle helps improve traction and reduce unnecessary load on the chassis.

### Centre of Gravity

Keeping heavier components low helps improve stability during sharp turns and rapid changes in direction.

### Chassis Design

A lightweight acrylic or aluminium chassis helps reduce overall mass while maintaining structural rigidity.

### Motor Selection

The motor RPM and torque characteristics should be selected according to the track layout and required acceleration.

### Wheel Traction

High-grip wheels help reduce slipping and improve directional control during acceleration and cornering.

## Development Considerations

Some important considerations when developing and tuning the robot include:

- **Motor Calibration:** Account for differences in motor speed and response.
- **PWM Tuning:** Adjust speed settings to achieve predictable movement.
- **Wireless Latency:** Minimize communication delays for responsive control.
- **Power Management:** Ensure the battery can supply sufficient current under motor load.
- **Electrical Noise:** Use appropriate power distribution and grounding practices.
- **Mechanical Stability:** Maintain a low centre of gravity and balanced weight distribution.
- **Firmware Modularity:** Keep communication and motor control logic separated where possible.

## Future Improvements

Potential improvements to the project include:

- Adding closed-loop motor speed control using encoders.
- Implementing PID-based motion control.
- Integrating IMU-based orientation feedback.
- Adding autonomous line-following capabilities.
- Implementing obstacle detection and avoidance.
- Improving wireless communication reliability.
- Adding battery voltage monitoring.
- Developing a more advanced speed-control interface.
- Optimizing chassis geometry and weight distribution.

## Technologies Used

- **Embedded C/C++** – Microcontroller firmware development.
- **Raspberry Pi Pico W** – Microcontroller platform.
- **ESP8266 / ESP-01** – Wireless communication platform.
- **L298N** – Dual H-bridge motor driver.
- **PWM** – Motor speed regulation.
- **DC Gear Motors** – Robot locomotion.
- **MPU-6050** – Motion sensing.

## Acknowledgements

Developed as a robotics project for XLR8 2026 competition participation and won.

---

**Repository:** [pranav398/xlr8-bot](https://github.com/pranav398/xlr8-bot)

**Author:** [@pranav398](https://github.com/pranav398)

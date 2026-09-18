# Smart Environmental Monitor & Controller – ESP32

An IoT-based smart environmental monitoring and control system developed using **ESP32**, **Blynk IoT**, multiple sensors, actuators, and Object-Oriented Programming concepts.

The system continuously monitors environmental conditions and can automatically control different actuators based on sensor readings. It also supports remote monitoring and manual control through the Blynk dashboard.

---

## Project Features

- Temperature and humidity monitoring
- Soil moisture monitoring
- Light intensity monitoring
- Water level monitoring
- Automatic irrigation control
- Grow light control
- Ventilation control
- OLED real-time display
- Blynk IoT integration
- Auto and Manual operating modes
- OOP-based software architecture
- Observer design pattern
- Non-blocking control logic
- Power management using ESP32
- Remote monitoring and control

---

## Hardware Components

- ESP32 DevKit V1
- DHT11 Temperature & Humidity Sensor
- Soil Moisture Sensor
- LDR Light Sensor
- Water Level Sensor
- SSD1306 OLED Display
- ULN2003 Stepper Motor Driver
- Stepper Motor
- Relay Module for Grow Light
- Relay Module for Water Pump
- Water Pump
- Refill Indicator LED
- External Power Supply
- Custom PCB

---

## ESP32 Pin Configuration

| Component | ESP32 GPIO |
|---|---:|
| DHT11 | GPIO 4 |
| Soil Moisture Sensor | GPIO 34 |
| LDR | GPIO 35 |
| Water Level Sensor | GPIO 32 |
| Pump Relay | GPIO 13 |
| Grow Light Relay | GPIO 26 |
| Refill LED | GPIO 27 |
| Power Enable | GPIO 25 |
| Stepper IN1 | GPIO 16 |
| Stepper IN2 | GPIO 17 |
| Stepper IN3 | GPIO 18 |
| Stepper IN4 | GPIO 19 |
| OLED SDA | GPIO 21 |
| OLED SCL | GPIO 22 |

---

## Blynk Virtual Pins

| Virtual Pin | Function |
|---|---|
| V0 | Temperature |
| V1 | Humidity |
| V2 | Soil Moisture |
| V3 | Light Level |
| V4 | Water Level |
| V5 | Grow Light Control |
| V6 | Water Pump Control |
| V7 | Vent Control |
| V8 | Auto / Manual Mode |

---

## Operating Modes

### Auto Mode

In Auto Mode, the ESP32 reads sensor values and automatically controls the actuators according to the predefined environmental conditions.

The automatic control system can manage:

- Irrigation
- Grow light
- Ventilation
- Water level protection

---

### Manual Mode

In Manual Mode, automatic actuator control is disabled.

The user can remotely control:

- Water pump
- Grow light
- Ventilation

through the Blynk IoT dashboard.

---

## IoT Integration

The project uses **Blynk IoT** for remote monitoring and control.

Sensor readings are sent to the Blynk dashboard, allowing the user to monitor the system remotely.

The dashboard displays:

- Temperature
- Humidity
- Soil moisture
- Light level
- Water level

It also provides manual controls for the system actuators.

---

## Software Architecture

The project was developed using Object-Oriented Programming in C++.

The code is organized into multiple classes to improve:

- Modularity
- Reusability
- Maintainability
- Code organization

The system also implements the **Observer Design Pattern** for communication between environmental monitoring and actuator control.

---

## Power Management

The ESP32 controls a master power-enable line using:

`GPIO 25`

The system follows a power management sequence:

1. Enable system power
2. Wait for sensors to stabilize
3. Read environmental data
4. Update actuators
5. Send IoT data
6. Disable external system power
7. Enter low-power sleep
8. Wake up and repeat

This reduces unnecessary power consumption.

---

## Sensors

The system monitors four main environmental parameters:

### Temperature and Humidity

Measured using the DHT11 sensor.

### Soil Moisture

Used to determine whether irrigation is required.

### Light Level

Measured using an LDR sensor.

### Water Level

Used to protect the irrigation pump from operating when the water level is too low.

---

## Actuators

The system controls several actuators:

- Water Pump
- Grow Light
- Ventilation Stepper Motor
- Refill Status LED

---

## Display

A 128×64 SSD1306 OLED display is used to show system information and sensor readings locally.

Communication is performed through I2C:

- SDA: GPIO 21
- SCL: GPIO 22

---

## Technologies Used

- ESP32
- Arduino Framework
- C++
- Object-Oriented Programming
- Observer Design Pattern
- Blynk IoT
- Wi-Fi
- ADC
- I2C
- Embedded Systems
- PCB Design

---

## Development Tools

- Arduino IDE
- Blynk IoT
- Proteus
- EasyEDA
- GitHub

---

## Main Project Code

The complete ESP32 source code is available in this repository:

`Smart_environmental_monitor_esp32.ino`

---


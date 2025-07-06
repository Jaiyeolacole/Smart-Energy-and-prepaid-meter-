# Smart-Energy-and-prepaid-meter-
# Power Monitoring and Control System

## Overview
This ESP32-based system monitors electrical parameters using a PZEM-004T energy meter, logs data to an SD card, displays real-time information on an LCD, and allows remote control via API endpoints. The system can switch relays based on commands from a cloud server while continuously monitoring voltage, current, power, and energy consumption.

## Features
- Real-time monitoring of AC electrical parameters:
  - Voltage (V)
  - Current (A)
  - Power (W)
  - Energy (kWh)
- Data logging to SD card (CSV format)
- LCD display showing measurements and system status
- WiFi connectivity with automatic reconnection
- Secure HTTPS API communication
- Remote relay control via cloud API
- NTP time synchronization for accurate timestamps

## Hardware Requirements
- ESP32 development board
- PZEM-004T AC energy meter
- 16x2 I2C LCD display
- MicroSD card module
- 2-channel relay module
- Breadboard and jumper wires
- 5V power supply

## Wiring Diagram
```
ESP32       PZEM-004T       LCD       SD Card       Relay
---------------------------------------------------------
3.3V  ----- VCC
GND   ----- GND
GPIO25 --- RX
GPIO26 --- TX

GPIO21 --- SDA
GPIO22 --- SCL

GPIO5  --- CS
GPIO23 --- MOSI
GPIO18 --- MISO
GPIO19 --- SCK

GPIO27 --- IN1
GPIO28 --- IN2
```

## Software Requirements
- Arduino IDE
- Required libraries:
  - PZEM004Tv30
  - LiquidCrystal_I2C
  - SD
  - WiFi
  - WiFiClientSecure
  - HTTPClient
  - ArduinoJson
  - time

## Installation
1. Clone this repository or download the source code
2. Open the project in Arduino IDE
3. Install all required libraries
4. Update WiFi credentials in the code
5. Upload to your ESP32 board

## Configuration
Modify these constants in the code as needed:
```cpp
// WiFi credentials
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// API endpoints
const char* serverName = "https://endpoint.com/send_data";
const char* serverResponse = "https://endpoint.com/get_device_mode";

// Timing intervals (milliseconds)
const unsigned long logInterval = 10000; // 10 seconds
const unsigned long commandCheckInterval = 5000; // 5 seconds
```

## Usage
1. Power on the system
2. The LCD will display initialization messages
3. Once connected to WiFi, it will begin monitoring
4. Data is automatically logged to the SD card
5. Relay states can be controlled via the API endpoint

## API Documentation
### Send Data Endpoint (POST)
```
https://api-endpoint.com/send_data
```
Payload format:
```json
{
  "voltage": 230.5,
  "current": 1.25,
  "power": 287.5,
  "energy": 1250.75,
  "time_stamp": "2023-06-15T14:30:00Z",
  "relay_state": "on"
}
```

### Get Command Endpoint (GET)
```
https://api-endpoint.com/get_device_mode
```
Response format (simple boolean or JSON):
```
true/false
```
or
```json
{
  "status": "on"
}
```

## Troubleshooting
- **Sensor not reading**: Check PZEM connections and AC wiring
- **WiFi not connecting**: Verify credentials and signal strength
- **SD card errors**: Ensure proper formatting (FAT32) and connections
- **API failures**: Check network connectivity and endpoint URLs



## Author
[Jaiyeola Emmanuel]  
[+2347063135499]  

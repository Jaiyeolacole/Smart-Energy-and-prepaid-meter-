#include <PZEM004Tv30.h>
#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

// WiFi credentials
const char* ssid = "Ceejay";
const char* password = "nopassword";

// API endpoints
const char* serverName = "https://meterryapis.onrender.com/esp/send_data";
const char* serverResponse = "https://meterryapis.onrender.com/esp/get_device_mode";

// LCD Configuration
#define I2C_SDA 21
#define I2C_SCL 22
LiquidCrystal_I2C lcd(0x27, 16, 2);

// SD Card Configuration
const int chipSelect = 5;

// Relay pins
#define RELAY_PIN1 27
#define RELAY_PIN2 28  

// PZEM Configuration
#define PZEM_RX_PIN 25
#define PZEM_TX_PIN 26
PZEM004Tv30 pzem(Serial2, PZEM_RX_PIN, PZEM_TX_PIN);

// Timing
unsigned long lastScroll = 0;
unsigned long lastLogTime = 0;
unsigned long lastCommandCheck = 0;
unsigned long lastTimeSync = 0;
const unsigned long scrollInterval = 2000;
const unsigned long logInterval = 10000; // 10 seconds
const unsigned long commandCheckInterval = 5000; // 5 seconds
const unsigned long timeSyncInterval = 3600000; // 1 hour

// System State
unsigned long runtime = 0;
bool sdCardFailed = false;
bool sensorErrorState = false;
bool relayState = false;
bool timeSynced = false;

// Measurement variables
float voltage = 0.0;
float current = 0.0;
float power = 0.0;
float energy = 0.0;

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, PZEM_RX_PIN, PZEM_TX_PIN);

  // Initialize hardware
  pinMode(RELAY_PIN1, OUTPUT);
  pinMode(RELAY_PIN2, OUTPUT);
  digitalWrite(RELAY_PIN1, HIGH); // Start with relay off
  digitalWrite(RELAY_PIN2, HIGH);

  // Initialize LCD
  Wire.begin(I2C_SDA, I2C_SCL);
  lcd.init();
  lcd.backlight();
  lcd.print("Power Monitor");
  delay(2000);
  lcd.clear();

  // Connect to Wi-Fi
  connectToWiFi();

  // Initialize time synchronization
  configTime(0, 0, "pool.ntp.org");

  // Initialize PZEM
  initializePZEM();

  // Initialize SD Card
  initSDCard();

  lcd.print("System Ready");
  delay(2000);
  lcd.clear();
}

void connectToWiFi() {
  lcd.print("WiFi Connecting");
  Serial.print("Connecting to WiFi");
  
  WiFi.begin(ssid, password);
  
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 15000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected!");
    lcd.clear();
    lcd.print("WiFi Connected");
    delay(1000);
  } else {
    Serial.println("\nWiFi Failed!");
    lcd.clear();
    lcd.print("WiFi Failed");
    delay(2000);
  }
  lcd.clear();
}

void syncTime() {
  if (WiFi.status() != WL_CONNECTED) return;

  configTime(0, 0, "pool.ntp.org");
  
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    timeSynced = false;
    return;
  }
  
  timeSynced = true;
  Serial.println("Time synchronized");
}

String getISOTimestamp() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "1970-01-01T00:00:00Z";
  }

  char timeStr[25];
  strftime(timeStr, sizeof(timeStr), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
  return String(timeStr);
}

void initializePZEM() {
  lcd.print("Initializing PZEM");
  delay(1000);
  
  // Reset energy counter
  if (!pzem.resetEnergy()) {
    Serial.println("Failed to reset energy!");
    lcd.clear();
    lcd.print("PZEM Reset Fail");
    delay(2000);
  } else {
    Serial.println("PZEM initialized");
  }
  lcd.clear();
}

void initSDCard() {
  if (!SD.begin(chipSelect)) {
    lcd.print("SD Init Failed!");
    Serial.println("SD Card Init Failed!");
    sdCardFailed = true;
    delay(2000);
    lcd.clear();
    return;
  }

  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    lcd.print("No SD Card!");
    Serial.println("No SD card attached");
    sdCardFailed = true;
    delay(2000);
    lcd.clear();
    return;
  }

  Serial.print("SD Card Type: ");
  if (cardType == CARD_MMC) Serial.println("MMC");
  else if (cardType == CARD_SD) Serial.println("SDSC");
  else if (cardType == CARD_SDHC) Serial.println("SDHC");
  else Serial.println("UNKNOWN");

  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);

  File dataFile = SD.open("/powerlog.csv", FILE_WRITE);
  if (dataFile) {
    dataFile.println("Timestamp(ms),Voltage(V),Current(A),Power(W),Energy(Wh),Runtime(s),RelayState");
    dataFile.close();
    lcd.print("SD Card Ready");
    Serial.println("SD Card Initialized");
  } else {
    lcd.print("SD Write Fail!");
    Serial.println("SD Card write failed");
    sdCardFailed = true;
  }
  delay(1000);
  lcd.clear();
}

void loop() {
  // Sync time periodically
  if (millis() - lastTimeSync >= timeSyncInterval || !timeSynced) {
    syncTime();
    lastTimeSync = millis();
  }

  // Read sensor data
  readSensorData();

  // Handle display updates
  updateDisplay();

  // Handle periodic tasks
  unsigned long currentMillis = millis();
  
  // Log data periodically
  if (currentMillis - lastLogTime >= logInterval) {
    lastLogTime = currentMillis;
    logData();
    sendToServer();
  }

  // Check for commands periodically
  if (currentMillis - lastCommandCheck >= commandCheckInterval) {
    lastCommandCheck = currentMillis;
    checkForCommand();
  }

  delay(500); // Main loop delay
}

void readSensorData() {
  voltage = pzem.voltage();
  current = pzem.current();
  power = pzem.power();
  energy = pzem.energy();
  runtime = millis() / 1000;

  if (isnan(voltage) || isnan(current) || isnan(power) || isnan(energy)) {
    if (!sensorErrorState) {
      sensorErrorState = true;
      lcd.clear();
      lcd.print("Sensor Error!");
      Serial.println("PZEM read error - Check AC connection");
    }
    return;
  }

  if (sensorErrorState) {
    sensorErrorState = false;
    lcd.clear();
    Serial.println("Sensor recovered");
  }

  Serial.printf("V: %.1fV | I: %.3fA | P: %.1fW | E: %.3fWh | T: %lus | Relay: %s\n",
                voltage, current, power, energy, runtime, 
                relayState ? "ON" : "OFF");
}

void updateDisplay() {
  if (millis() - lastScroll < scrollInterval) return;
  lastScroll = millis();
  
  char line1[17];
  char line2[17];
  sprintf(line1, "V:%.1f I:%.3f", voltage, current);
  sprintf(line2, "P:%.1f E:%.3f", power, energy);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line1);
  lcd.setCursor(13, 0);
  lcd.print(relayState ? "ON" : "OFF");
  lcd.setCursor(0, 1);
  lcd.print(line2);
}

void logData() {
  if (!sdCardFailed) {
    File dataFile = SD.open("/powerlog.csv", FILE_APPEND);
    if (dataFile) {
      dataFile.printf("%lu,%.1f,%.3f,%.1f,%.3f,%lu,%d\n",
                      millis(), voltage, current, power, energy, runtime, relayState);
      dataFile.close();
    } else {
      Serial.println("SD Write Failed");
    }
  }
}

void sendToServer() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected. Skipping server update.");
    return;
  }

  // Skip if we have invalid sensor readings
  if (isnan(voltage) || isnan(current) || isnan(power) || isnan(energy)) {
    Serial.println("Invalid sensor readings. Skipping server update.");
    return;
  }

  WiFiClientSecure client;
  client.setInsecure(); // For testing only - use certificates in production

  HTTPClient http;
  if (!http.begin(client, serverName)) {
    Serial.println("Failed to begin HTTP connection");
    return;
  }

  http.addHeader("Content-Type", "application/json");

  DynamicJsonDocument jsonDoc(256);
  jsonDoc["voltage"] = voltage;
  jsonDoc["current"] = current;
  jsonDoc["power"] = power;
  jsonDoc["energy"] = energy;
  jsonDoc["time_stamp"] = getISOTimestamp();
  jsonDoc["relay_state"] = relayState ? "on" : "off";

  String jsonData;
  serializeJson(jsonDoc, jsonData);

  Serial.print("Sending: ");
  serializeJson(jsonDoc, Serial);
  Serial.println();

  int httpResponseCode = http.POST(jsonData);

  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.print("Server response: ");
    Serial.println(response);
  } else {
    Serial.print("Error sending POST: ");
    Serial.println(http.errorToString(httpResponseCode));
  }

  http.end();
}

void checkForCommand() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected. Skipping command check.");
    return;
  }

  WiFiClientSecure client;
  client.setInsecure(); // For testing only - use certificates in production

  HTTPClient http;
  if (!http.begin(client, serverResponse)) {
    Serial.println("Failed to begin HTTP connection");
    return;
  }

  http.addHeader("accept", "application/json");
  int httpResponseCode = http.GET();

  if (httpResponseCode == HTTP_CODE_OK) {
    String payload = http.getString();
    payload.trim();
    Serial.print("API Response: ");
    Serial.println(payload);

    // Handle simple true/false response
    if (payload == "true" || payload == "false") {
      bool newRelayState = (payload == "true");
      if (newRelayState != relayState) {
        relayState = newRelayState;
        digitalWrite(RELAY_PIN1, relayState ? LOW : HIGH);
        Serial.println(relayState ? "Relay turned ON" : "Relay turned OFF");
      }
      return;
    }

    // Parse JSON response if not simple true/false
    DynamicJsonDocument doc(128);
    DeserializationError error = deserializeJson(doc, payload);

    if (!error) {
      if (doc.containsKey("status")) {
        String status = doc["status"].as<String>();
        bool newRelayState = (status == "on" || status == "true");
        
        if (newRelayState != relayState) {
          relayState = newRelayState;
          digitalWrite(RELAY_PIN1, relayState ? LOW : HIGH);
          Serial.println(relayState ? "Relay turned ON" : "Relay turned OFF");
        }
      }
    } else {
      Serial.print("JSON parsing failed: ");
      Serial.println(error.c_str());
    }
  } else {
    Serial.print("Error fetching API: ");
    Serial.println(httpResponseCode);
  }

  http.end();
}
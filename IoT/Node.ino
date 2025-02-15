// Include required libraries
#include <esp_now.h>
#include <WiFi.h>
#include <random>

// COPY WIFI SETTINGS FROM GATEWAY
const char* ssid = "Direktorat Kemendikbud";
const char* password = "NadiemGantengSih";

// CHANGE THIS TO MATCH YOUR GATEWAY'S MAC ADDRESS
uint8_t gatewayAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};  // Replace with actual Gateway MAC

// PIN DEFINITIONS
#define MQ2_PIN 34          // Analog pin connected to MQ2 AO
#define READING_INTERVAL 1000

// SENSOR CALIBRATION VALUES - SAME AS GATEWAY
#define VOLTAGE_RESOLUTION 3.3    
#define ADC_RESOLUTION 4095.0     
#define R0_CLEAN_AIR 9.83        
#define MQ2_RL 5.0               

// Structure to hold sensor data - MUST MATCH GATEWAY
struct SensorData {
    float ammoniaLevel;
    uint8_t sensorID;
    uint32_t readingNumber;
};

SensorData sensorData;
float lastReading = 0;
unsigned long lastReadingTime = 0;
bool isCalibrated = false;
float R0 = R0_CLEAN_AIR;

void setup() {
    Serial.begin(115200);
    Serial.println("SENSOR NODE B STARTING UP");

    // Initialize MQ2 pin
    pinMode(MQ2_PIN, INPUT);
    
    // Warmup and calibration
    Serial.println("WARMING UP MQ2 SENSOR...");
    delay(20000);  // 20 second warmup
    
    // Quick calibration
    calibrateSensor();

    // Set up ESP-NOW
    WiFi.mode(WIFI_STA);
    
    if (esp_now_init() != ESP_OK) {
        Serial.println("FAILED TO START ESP-NOW. HALTING.");
        while (1);
    }

    esp_now_register_send_cb(OnDataSent);

    // Add gateway as peer
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, gatewayAddress, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("FAILED TO ADD GATEWAY AS PEER. HALTING.");
        while (1);
    }

    // Initialize sensor data
    sensorData.sensorID = 2;  // This is Trash Can B
    sensorData.readingNumber = 0;
}

// Copy these functions from Gateway.ino
void calibrateSensor() {
    Serial.println("CALIBRATING SENSOR...");
    float avgResistance = 0;
    for(int i = 0; i < 10; i++) {
        avgResistance += getResistanceRatio();
        delay(1000);
    }
    R0 = avgResistance / 10.0;
    isCalibrated = true;
    Serial.printf("CALIBRATION COMPLETE. R0 = %.2f\n", R0);
}

float getResistanceRatio() {
    float rawADC = analogRead(MQ2_PIN);
    float voltageOut = (rawADC * VOLTAGE_RESOLUTION) / ADC_RESOLUTION;
    float Rs = ((VOLTAGE_RESOLUTION * MQ2_RL) / voltageOut) - MQ2_RL;
    return Rs / R0;
}

float readAmmonia() {
    if (!isCalibrated) return 0;
    
    float ratio = getResistanceRatio();
    float ppm = 102.2 * pow(ratio, -2.473);
    return ppm;
}

void loop() {
    if (millis() - lastReadingTime >= READING_INTERVAL) {
        // Read actual sensor
        float newReading = readAmmonia();
        
        // Update data structure
        sensorData.ammoniaLevel = newReading;
        sensorData.readingNumber++;
        lastReading = newReading;

        // Send to gateway
        esp_err_t result = esp_now_send(gatewayAddress, 
                                      (uint8_t *) &sensorData, 
                                      sizeof(SensorData));

        if (result == ESP_OK) {
            Serial.printf("SENT READING: %.2f PPM\n", newReading);
        } else {
            Serial.println("FAILED TO SEND READING");
        }

        lastReadingTime = millis();
    }
}

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    Serial.print("Last Packet Send Status: ");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "SUCCESS" : "FAILED");
}

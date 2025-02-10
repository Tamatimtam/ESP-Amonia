
// Include required libraries
#include <esp_now.h>
#include <WiFi.h>

// CHANGE THIS TO MATCH YOUR GATEWAY'S MAC ADDRESS
uint8_t gatewayAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Pin where MQ2 sensor is connected
#define MQ2_PIN 34
// How often to take readings (in milliseconds)
#define READING_INTERVAL 30000

// Structure to hold sensor data
struct SensorData {
    float ammoniaLevel;  // Ammonia reading
    uint8_t sensorID;    // Unique ID for this sensor
    uint32_t readingNumber; // Counter to track missed readings
};

// Create a data object
SensorData sensorData;

// Track when we last sent a reading
unsigned long lastReadingTime = 0;

void setup() {
    // Start serial communication - lets us debug using the Serial Monitor
    Serial.println("SENSOR NODE STARTING UP");
    Serial.begin(115200);

    // Set this device as a WiFi station
    WiFi.mode(WIFI_STA);

    // Start ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("FAILED TO START ESP-NOW. HALTING.");
        while (1);
    }

    // Register the function that will handle sending results
    esp_now_register_send_cb(OnDataSent);

    // Add the gateway as a peer device
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, gatewayAddress, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("FAILED TO ADD GATEWAY AS PEER. HALTING.");
        while (1);
    }

    // Initialize our sensor data
    sensorData.sensorID = 1;  // CHANGE THIS FOR EACH SENSOR NODE
    sensorData.readingNumber = 0;
}

void loop() {
    // Check if it's time to take a new reading
    if (millis() - lastReadingTime >= READING_INTERVAL) {
        // Read the sensor
        float sensorValue = analogRead(MQ2_PIN);
        
        // Convert raw reading to PPM (parts per million)
        // NOTE: This conversion needs calibration for your specific sensor
        float ammoniaPPM = sensorValue * (5.0 / 1023.0);
        
        // Update our data structure
        sensorData.ammoniaLevel = ammoniaPPM;
        sensorData.readingNumber++;

        // Send the data to the gateway
        esp_err_t result = esp_now_send(gatewayAddress, 
                                      (uint8_t *) &sensorData, 
                                      sizeof(SensorData));

        if (result == ESP_OK) {
            Serial.println("SENT READING SUCCESSFULLY");
        } else {
            Serial.println("FAILED TO SEND READING");
        }

        // Update the last reading time
        lastReadingTime = millis();
    }
}

// This function runs whenever we try to send data
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    Serial.print("Last Packet Send Status: ");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "SUCCESS" : "FAILED");
}

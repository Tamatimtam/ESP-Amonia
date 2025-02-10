// Include required libraries
#include <esp_now.h>
#include <WiFi.h>
#include <random>

// CHANGE THIS TO MATCH YOUR GATEWAY'S MAC ADDRESS
uint8_t gatewayAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Pin where MQ2 sensor is connected
#define MQ2_PIN 34
// How often to take readings (in milliseconds)
#define READING_INTERVAL 30000

#define MIN_AMMONIA 0.0    // Minimum ammonia reading in PPM
#define MAX_AMMONIA 50.0   // Maximum ammonia reading in PPM
#define BASE_AMMONIA 5.0   // Base level of ammonia
#define VARIATION 3.0      // How much readings can vary

float lastReading = BASE_AMMONIA;  // Keep track of last reading for realistic variations

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
    sensorData.sensorID = 2;  // This is Trash Can B (changed from 1)
    sensorData.readingNumber = 0;
}

void loop() {
    if (millis() - lastReadingTime >= READING_INTERVAL) {
        // Generate a realistic random ammonia reading
        // Each new reading varies slightly from the last one
        float variation = ((float)random(0, 1000) / 1000.0) * VARIATION;
        if (random(2) == 0) variation = -variation;  // 50% chance of going up or down
        
        float newReading = lastReading + variation;
        // Keep reading within realistic bounds
        newReading = max(MIN_AMMONIA, min(MAX_AMMONIA, newReading));
        
        // Update our data structure
        sensorData.ammoniaLevel = newReading;
        sensorData.readingNumber++;
        lastReading = newReading;  // Save for next iteration

        // Send the data to the gateway
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

// This function runs whenever we try to send data
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    Serial.print("Last Packet Send Status: ");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "SUCCESS" : "FAILED");
}

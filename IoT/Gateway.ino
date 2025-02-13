// LIBRARIES WE NEED
#include <esp_now.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <random>

// WIFI SETTINGS - CHANGE THESE TO MATCH YOUR NETWORK
const char* ssid = "padahal katanya uangtakan kemana";
const char* password = "jika memang rejeki akan ditransfer juga";

// MQTT SETTINGS - CHANGE THESE TO MATCH YOUR SETUP
const char* mqttServer = "broker.emqx.io";
const int mqttPort = 1883;
const char* mqttUser = "";    // public broker doesn't need credentials
const char* mqttPassword = "";
// Add a unique client ID to avoid conflicts with other users
const char* mqttClientId = "AmmoniacGateway123";  // Change 123 to random numbers

// THIS IS THE SAME DATA STRUCTURE AS THE NODES USE
struct SensorData {
    float ammoniaLevel;
    uint8_t sensorID;
    uint32_t readingNumber;
};

// PIN DEFINITIONS
#define MQ2_PIN 34          // Analog pin connected to MQ2 AO
#define READING_INTERVAL 1000     // Read every second during testing

// SENSOR CALIBRATION VALUES
#define VOLTAGE_RESOLUTION 3.3    // ESP32 runs on 3.3V
#define ADC_RESOLUTION 4095.0     // 12-bit ADC
#define R0_CLEAN_AIR 1.0         // Decreased from 9.83 to make more sensitive
#define MQ2_RL 1.0               // Decreased load resistance for better sensitivity

SensorData localSensor;  // For Gateway's own sensor (Trash Can A)
float lastReading = 0;
unsigned long lastReadingTime = 0;
bool isCalibrated = false;
float R0 = R0_CLEAN_AIR;  // Will be updated during calibration

// MQTT CLIENT SETUP
WiFiClient espClient;
PubSubClient mqttClient(espClient);

void setup() {
    // START SERIAL FOR DEBUGGING
    Serial.begin(115200);
    Serial.println("GATEWAY STARTING UP");

    // CONNECT TO WIFI
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.println("TRYING TO CONNECT TO WIFI...");
    }
    Serial.println("CONNECTED TO WIFI");

    // PRINT THE GATEWAY'S MAC ADDRESS - YOU'LL NEED THIS FOR THE NODES
    Serial.print("GATEWAY MAC ADDRESS: ");
    Serial.println(WiFi.macAddress());

    // START ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("FAILED TO START ESP-NOW. HALTING.");
        while (1);
    }

    // SET UP FUNCTION TO HANDLE INCOMING DATA
    esp_now_register_recv_cb(OnDataReceived);

    // CONNECT TO MQTT SERVER
    mqttClient.setServer(mqttServer, mqttPort);
    connectToMqtt();

    // Initialize MQ2 pin
    pinMode(MQ2_PIN, INPUT);
    
    // Warmup and calibration
    Serial.println("WARMING UP MQ2 SENSOR...");
    delay(20000);  // 20 second warmup
    
    // Quick calibration
    calibrateSensor();
}

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

// Modify readAmmonia() to be more sensitive
float readAmmonia() {
    if (!isCalibrated) return 0;
    
    float ratio = getResistanceRatio();
    // More sensitive PPM calculation
    float ppm = 50.0 * pow(ratio, -1.8);  // Adjusted values for better sensitivity
    
    // Apply some smoothing
    static float lastPPM = 0;
    ppm = (ppm * 0.7) + (lastPPM * 0.3);  // 70% new reading, 30% old reading
    lastPPM = ppm;
    
    return ppm;
}

// Add these debug functions
void debugReadings() {
    float rawADC = analogRead(MQ2_PIN);
    float voltage = (rawADC * VOLTAGE_RESOLUTION) / ADC_RESOLUTION;
    float ratio = getResistanceRatio();
    float ppm = readAmmonia();
    
    Serial.println("\n=== SUPER DETAILED DEBUG ===");
    Serial.printf("Raw ADC: %.0f (should be 0-4095)\n", rawADC);
    Serial.printf("Voltage: %.3fV (should be 0-3.3V)\n", voltage);
    Serial.printf("Rs/R0 Ratio: %.3f\n", ratio);
    Serial.printf("Calculated PPM: %.2f\n", ppm);
    
    // Add threshold indicators
    Serial.println("\nREADING STRENGTH:");
    if (rawADC < 100) Serial.println("⚠️ WARNING: Reading too low, check connections");
    if (rawADC > 4000) Serial.println("⚠️ WARNING: Reading too high, check connections");
    
    // Visual bar graph
    Serial.print("Signal Strength: ");
    int bars = map(rawADC, 0, 4095, 0, 20);
    for(int i = 0; i < bars; i++) Serial.print("█");
    for(int i = bars; i < 20; i++) Serial.print("░");
    Serial.println();
    Serial.println("========================\n");
}

void loop() {
    // Check if it's time for a new local reading
    if (millis() - lastReadingTime >= READING_INTERVAL) {
        debugReadings();  // Add this line
        // Read actual sensor instead of random
        float newReading = readAmmonia();
        
        localSensor.ammoniaLevel = newReading;
        localSensor.sensorID = 1;  // This is Trash Can A
        localSensor.readingNumber++;
        lastReading = newReading;

        // Create JSON and publish to MQTT
        char jsonBuffer[200];
        snprintf(jsonBuffer, sizeof(jsonBuffer),
            "{\"sensor_id\":%d,\"ammonia\":%.2f,\"reading\":%d}",
            localSensor.sensorID,
            localSensor.ammoniaLevel,
            localSensor.readingNumber
        );

        // Changed topic to match Python app's subscription
        if (mqttClient.publish("amoniac/sensor/1", jsonBuffer)) {
            Serial.printf("SENT LOCAL READING: %.2f PPM\n", newReading);
        }

        lastReadingTime = millis();
    }

    // Keep MQTT connection alive
    if (!mqttClient.connected()) {
        connectToMqtt();
    }
    mqttClient.loop();
}

// THIS RUNS EVERY TIME WE GET DATA FROM A SENSOR
void OnDataReceived(const uint8_t *mac_addr, const uint8_t *data, int len) {
    // CONVERT THE INCOMING DATA TO OUR STRUCTURE
    SensorData* sensorData = (SensorData*) data;

    // CREATE A JSON STRING WITH THE DATA
    char jsonBuffer[200];
    snprintf(jsonBuffer, sizeof(jsonBuffer),
        "{\"sensor_id\":%d,\"ammonia\":%.2f,\"reading\":%d}",
        sensorData->sensorID,
        sensorData->ammoniaLevel,
        sensorData->readingNumber
    );

    // MAKE THE MQTT TOPIC NAME BASED ON SENSOR ID
    char topic[50];
    snprintf(topic, sizeof(topic), "amoniac/sensor%d", sensorData->sensorID);

    // SEND TO MQTT SERVER
    if (mqttClient.publish(topic, jsonBuffer)) {
        Serial.println("DATA SENT TO MQTT:");
        Serial.println(jsonBuffer);
    } else {
        Serial.println("FAILED TO SEND TO MQTT!");
    }
}

// CONNECTS OR RECONNECTS TO MQTT
void connectToMqtt() {
    while (!mqttClient.connected()) {
        Serial.println("CONNECTING TO MQTT...");
        
        if (mqttClient.connect(mqttClientId)) {  // Changed to use clientId
            Serial.println("CONNECTED TO MQTT");
        } else {
            Serial.print("MQTT CONNECTION FAILED! ERROR CODE: ");
            Serial.println(mqttClient.state());
            delay(2000);
        }
    }
}

// LIBRARIES WE NEED
#include <esp_now.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <random>

// WIFI SETTINGS - CHANGE THESE TO MATCH YOUR NETWORK
const char* ssid = "YOUR_WIFI_NAME";
const char* password = "YOUR_WIFI_PASSWORD";

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

#define READING_INTERVAL 30000
#define MIN_AMMONIA 0.0
#define MAX_AMMONIA 50.0
#define BASE_AMMONIA 5.0
#define VARIATION 3.0

SensorData localSensor;  // For Gateway's own sensor (Trash Can A)
float lastReading = BASE_AMMONIA;
unsigned long lastReadingTime = 0;

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
}

void loop() {
    // Check if it's time for a new local reading
    if (millis() - lastReadingTime >= READING_INTERVAL) {
        // Generate simulated reading for Trash Can A
        float variation = ((float)random(0, 1000) / 1000.0) * VARIATION;
        if (random(2) == 0) variation = -variation;
        
        float newReading = lastReading + variation;
        newReading = max(MIN_AMMONIA, min(MAX_AMMONIA, newReading));
        
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

        // Publish to MQTT
        if (mqttClient.publish("amoniac/sensor1", jsonBuffer)) {
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

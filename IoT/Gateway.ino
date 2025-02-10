
// LIBRARIES WE NEED
#include <esp_now.h>
#include <WiFi.h>
#include <PubSubClient.h>

// WIFI SETTINGS - CHANGE THESE TO MATCH YOUR NETWORK
const char* ssid = "YOUR_WIFI_NAME";
const char* password = "YOUR_WIFI_PASSWORD";

// MQTT SETTINGS - CHANGE THESE TO MATCH YOUR SETUP
const char* mqttServer = "YOUR_MQTT_SERVER_IP";
const int mqttPort = 1883;
const char* mqttUser = "YOUR_MQTT_USERNAME";
const char* mqttPassword = "YOUR_MQTT_PASSWORD";

// THIS IS THE SAME DATA STRUCTURE AS THE NODES USE
struct SensorData {
    float ammoniaLevel;
    uint8_t sensorID;
    uint32_t readingNumber;
};

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
    // KEEP MQTT CONNECTION ALIVE
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
        
        if (mqttClient.connect("ESP32Gateway", mqttUser, mqttPassword)) {
            Serial.println("CONNECTED TO MQTT");
        } else {
            Serial.print("MQTT CONNECTION FAILED! ERROR CODE: ");
            Serial.println(mqttClient.state());
            delay(2000);
        }
    }
}

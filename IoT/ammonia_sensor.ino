
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// Network credentials
const char* ssid = "Direktorat Kemendikbud";
const char* password = "NadiemGantengSih";

// MQTT Broker settings
const char* mqtt_broker = "broker.emqx.io";
const int mqtt_port = 1883;

// Pin definitions
const int TRASH_CAN_A_PIN = 34;  // ADC pin for first sensor
const int TRASH_CAN_B_PIN = 35;  // ADC pin for second sensor

// Constants for reading stabilization
const float ALPHA = 0.2;  // Smoothing factor (0-1), lower = more smoothing
const int WARMUP_TIME = 20;  // Sensor warmup time in seconds
const float MAX_PPM = 25.0;  // Maximum PPM value
const int SAMPLES_PER_READ = 10;  // Number of samples to average
const int SAMPLE_DELAY = 100;  // Delay between samples in milliseconds

// Variables for reading smoothing
float smoothedA = 0;
float smoothedB = 0;

WiFiClient espClient;
PubSubClient client(espClient);

void setup() {
  Serial.begin(115200);
  
  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");

  // Configure MQTT
  client.setServer(mqtt_broker, mqtt_port);

  // Wait for sensor warmup
  Serial.println("Warming up sensors...");
  delay(WARMUP_TIME * 1000);
  
  // Initial calibration
  smoothedA = getStableReading(TRASH_CAN_A_PIN);
  smoothedB = getStableReading(TRASH_CAN_B_PIN);
}

float getStableReading(int pin) {
  float sum = 0;
  int validSamples = 0;
  
  // Take multiple samples
  for(int i = 0; i < SAMPLES_PER_READ; i++) {
    float reading = analogRead(pin);
    
    // Basic error checking
    if (!isnan(reading) && !isinf(reading) && reading > 0) {
      sum += reading;
      validSamples++;
    }
    delay(SAMPLE_DELAY);
  }
  
  // Return average if we have valid samples, otherwise return 0
  return (validSamples > 0) ? (sum / validSamples) : 0;
}

float convertToPPM(float reading) {
  // Convert ADC reading to voltage (3.3V reference)
  float voltage = (reading * 3.3) / 4095.0;
  
  // Simple linear conversion (you might need to adjust these values)
  float ppm = voltage * 10.0;  // Simplified conversion
  
  // Cap at MAX_PPM
  return min(ppm, MAX_PPM);
}

void publishReading(int sensorId, float ppm) {
  if (!client.connected()) {
    reconnect();
  }
  
  StaticJsonDocument<200> doc;
  doc["sensor_id"] = sensorId;
  doc["ammonia"] = ppm;
  
  char jsonBuffer[100];
  serializeJson(doc, jsonBuffer);
  
  String topic = "amoniac/sensor/" + String(sensorId);
  client.publish(topic.c_str(), jsonBuffer);
}

void reconnect() {
  while (!client.connected()) {
    String clientId = "ESP32Client-" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println("MQTT Connected");
    } else {
      delay(2000);
    }
  }
}

void loop() {
  // Get stable readings
  float currentA = getStableReading(TRASH_CAN_A_PIN);
  float currentB = getStableReading(TRASH_CAN_B_PIN);
  
  // Apply exponential moving average
  smoothedA = (ALPHA * currentA) + ((1.0 - ALPHA) * smoothedA);
  smoothedB = (ALPHA * currentB) + ((1.0 - ALPHA) * smoothedB);
  
  // Convert to PPM and ensure we're within limits
  float ppmA = convertToPPM(smoothedA);
  float ppmB = convertToPPM(smoothedB);
  
  // Publish readings
  publishReading(1, ppmA);  // Trash can A = sensor_id 1
  publishReading(2, ppmB);  // Trash can B = sensor_id 2
  
  // Debug output
  Serial.printf("A: %.2f PPM, B: %.2f PPM\n", ppmA, ppmB);
  
  delay(1000);  // Wait 1 second before next reading
}

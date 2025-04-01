#include <WiFi.h>           // Required for WiFi connectivity
#include <ArduinoJson.h>    // Library to handle JSON data
#include <NTPClient.h>      // NTP client library for time synchronization
#include "time.h"           // Time handling library
#include <WiFiUdp.h>        // UDP communication library
#include <PubSubClient.h>   // MQTT client library
#include <Wire.h>           // I2C communication library
#include <BH1750.h>         // Library for the BH1750 light sensor

// Initialize the BH1750 light sensor
BH1750 lightMeter;

// Declare WiFi and MQTT objects
WiFiClient espClient;
PubSubClient client(espClient);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP); // NTP client

// WiFi and MQTT configuration
const char* ssid = "Hrrd";                // WiFi SSID
const char* password = "ultc4302";     // WiFi password
const char* mqtt_server = "raspberrypi.local"; // MQTT server broker adress
const int mqtt_port = 1883;            // MQTT server port
const char* mqtt_topic = "can_serhat"; // MQTT topic
const char* mqtt_user = "iot";         // MQTT username
const char* mqtt_pass = "password";    // MQTT password

// Initialize WiFi connection
void initWiFi() {
  WiFi.mode(WIFI_STA);                 // Set WiFi to "Station" mode
  WiFi.begin(ssid, password);          // Connect to the WiFi network
  Serial.print("Connecting to ");
  Serial.println(ssid);

  // Wait until the WiFi connection is established
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }

  // Print connection details
  Serial.println("\nWiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  // Initialize NTP client for time synchronization
  timeClient.begin();
  timeClient.setTimeOffset(3600);     // Set timezone offset (GMT+1 = 3600 seconds)
}

// Reconnect to the MQTT server if disconnected
void reconnectMQTT() {
  while (!client.connected()) {       // Check if MQTT is disconnected
    Serial.println("Attempting MQTT connection...");
    if (client.connect("ESP8266Client", mqtt_user, mqtt_pass)) { // Attempt to connect
      Serial.println("Connected to MQTT server");
    } else {
      Serial.print("Failed to connect to MQTT server. Status: ");
      Serial.println(client.state());
      delay(2000); // Wait 2 seconds before retrying
    }
  }
}

// Get and format the current date and time
String getFormattedDateTime() {
  timeClient.update();                // Update the time from the NTP server
  unsigned long epochTime = timeClient.getEpochTime(); // Get Unix timestamp

  // Convert timestamp to tm structure
  time_t rawTime = (time_t)epochTime;
  struct tm* tmStruct = gmtime(&rawTime);

  // Format the date and time as a string
  char buffer[20];
  sprintf(buffer, "%04d-%02d-%02d %02d:%02d:%02d",
          tmStruct->tm_year + 1900,
          tmStruct->tm_mon + 1,
          tmStruct->tm_mday,
          tmStruct->tm_hour,
          tmStruct->tm_min,
          tmStruct->tm_sec);

  return String(buffer);  // Return the formatted date and time
}

void setup() {
  Serial.begin(9600);                // Initialize serial communication
  initWiFi();                        // Connect to WiFi
  Serial.print("RRSI: ");            // Print WiFi signal strength
  Serial.println(WiFi.RSSI());

  // Set up MQTT server connection
  client.setServer(mqtt_server, mqtt_port);
  reconnectMQTT();                   // Ensure MQTT connection is established

  // Initialize I2C communication and BH1750 sensor
  Wire.begin();
  lightMeter.begin();

  Serial.println(F("BH1750 Test"));  // Confirm BH1750 sensor initialization
}

void loop() {
  // Check WiFi connection and reconnect if needed
  if (WiFi.status() != WL_CONNECTED) {
    initWiFi();                      // Reconnect to WiFi if disconnected
  }

  // Check MQTT connection and reconnect if needed
  if (!client.connected()) {
    reconnectMQTT();                 // Reconnect to MQTT if disconnected
  }
  client.loop();                     // Process MQTT client tasks

  delay(2000);                       // Wait for 2 seconds

  // Read data from the BH1750 sensor
  float data = lightMeter.readLightLevel();
  if (isnan(data)) {                 // Handle sensor read errors
    Serial.println("Failed to read from sensor!");
    return;
  }

  // Get the current timestamp
  String timestamp = getFormattedDateTime();

  // Create JSON object with sensor data
  StaticJsonDocument<512> doc;
  doc["node_name"] = "can_serhat";   // Node name
  doc["sensor_type"] = "ambient_light"; // Sensor type
  doc["data"] = data;                // Light level data
  doc["timestamp"] = timestamp;      // Timestamp

  // Serialize JSON object to a character buffer
  char jsonBuffer[512];
  serializeJson(doc, jsonBuffer);

  // Print the JSON string to the serial monitor
  Serial.println(jsonBuffer);

  // Publish the JSON data to the MQTT topic
  if (client.publish(mqtt_topic, jsonBuffer)) {
    Serial.println("Message sent successfully");
  } else {
    Serial.println("Failed to send message");
  }
}

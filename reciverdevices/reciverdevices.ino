#include <SPI.h>
#include <LoRa.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>        // For WiFi connectivity
#include <ESP8266HTTPClient.h>  // For making HTTP requests to your API

// Declare a WiFiClient object globally.
// This is required for the updated HTTPClient::begin method.
WiFiClient client;

// --- WiFi Configuration ---
// IMPORTANT: Replace with your actual WiFi credentials
const char* ssid = "Politap";     // Your WiFi network name
const char* password = ""; // Your WiFi password

// --- API Configuration ---
// IMPORTANT: Replace with your laptop's static IP and API port/path
const char* apiHost = "10.207.1.142"; // Your laptop's Static IP address
const int apiPort = 3000;            // Your API's port
const char* apiPath = "/data";       // The endpoint path for your API (e.g., "/data", "/sensor_readings")

// LoRa Pin Definitions for ESP8266 (NodeMCU)
#define SS_PIN   15   // D8 (GPIO15)
#define RST_PIN  16   // D0 (GPIO16)
#define DIO0_PIN 5    // D1 (GPIO5)

void setup() {
  Serial.begin(115200);
  while (!Serial); // Wait for the Serial Monitor to open

  Serial.println("\n--- LoRa Receiver & API Sender ---");

  // --- LoRa Initialization ---
  LoRa.setPins(SS_PIN, RST_PIN, DIO0_PIN);
  if (!LoRa.begin(920E6)) { // Use 920MHz frequency for Indonesia
    Serial.println("❌ Failed to initialize LoRa");
    while (true); // Stay in an infinite loop if LoRa fails to start
  }
  LoRa.setSpreadingFactor(7);      // SF7-SF12
  LoRa.setSignalBandwidth(125E3);  // 125kHz bandwidth
  LoRa.setCodingRate4(5);          // 4/5 coding rate
  Serial.println("✅ LoRa is Ready at 920 MHz");

  // --- WiFi Connection ---
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ Connected to WiFi!");
  Serial.print("ESP8266 IP Address: ");
  Serial.println(WiFi.localIP());

  // Optional: Set a Static IP for ESP8266 (Uncomment and adjust if needed)
  /*
  IPAddress staticIP(192, 168, 1, 150); // Replace with your desired IP
  IPAddress gateway(192, 168, 1, 1);
  IPAddress subnet(255, 255, 255, 0);
  IPAddress dns(8, 8, 8, 8); // Google DNS
  if (!WiFi.config(staticIP, gateway, subnet, dns)) {
    Serial.println("❌ Failed to configure static IP.");
  } else {
    Serial.print("✅ Static IP set to: ");
    Serial.println(WiFi.localIP());
  }
  */

  Serial.println("--- Ready to Receive LoRa Data ---");
}

void loop() {
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String receivedPayload = "";
    while (LoRa.available()) {
      receivedPayload += (char)LoRa.read();
    }

    // Display raw payload with enhanced formatting
    Serial.println("\n📥 Raw LoRa Payload (Received):");
    Serial.println("=================================");
    Serial.println(receivedPayload);
    Serial.println("=================================");

    StaticJsonDocument<256> doc; // Adjust document size if your JSON grows significantly
    DeserializationError error = deserializeJson(doc, receivedPayload);

    if (error) {
      Serial.print("❌ Error parsing JSON: ");
      Serial.println(error.c_str());
      return; // Exit the function if parsing fails
    }

    // Extract and display the parsed data
    String no_seri = doc["no_seri"] | "UNKNOWN";
    float lat = doc["latitude"] | 0.0;
    float lng = doc["longitude"] | 0.0;
    float suhu = doc["suhu"] | 0.0;
    int kualitas_udara = doc["kualitas_udara"] | 0;

    Serial.println("✅ Deserialized Data:");
    Serial.println("Serial No      : " + no_seri);
    Serial.println("Latitude       : " + String(lat, 6));
    Serial.println("Longitude      : " + String(lng, 6));
    Serial.println("Temperature (°C): " + String(suhu, 1));
    Serial.println("Air Quality    : " + String(kualitas_udara));

    // --- Send data to API ---
    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      String serverUrl = "http://" + String(apiHost) + ":" + String(apiPort) + String(apiPath);

      http.begin(client, serverUrl);
      http.addHeader("Content-Type", "application/json");

      // Re-serialize the parsed JSON data to send to the API
      String jsonToSend;
      serializeJson(doc, jsonToSend);

      Serial.println("↗️ Sending data to API:");
      Serial.println(jsonToSend);

      int httpCode = http.POST(jsonToSend);

      if (httpCode > 0) {
        Serial.printf("✅ API Response Code: %d\n", httpCode);
        String apiResponsePayload = http.getString();
        Serial.println("API Response: " + apiResponsePayload);
      } else {
        Serial.printf("❌ Failed to send to API, Error: %s\n", http.errorToString(httpCode).c_str());
      }
      http.end(); // Close the HTTP connection
    } else {
      Serial.println("⚠️ WiFi is not connected, cannot send to API.");
      Serial.print("Attempting WiFi reconnection...");
      WiFi.begin(ssid, password);
      unsigned long startTime = millis();
      while (WiFi.status() != WL_CONNECTED && (millis() - startTime < 10000)) {
        delay(100);
        Serial.print(".");
      }
      if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n✅ WiFi reconnected successfully.");
      } else {
        Serial.println("\n❌ Failed to reconnect to WiFi.");
      }
    }
    Serial.println("------------------------------------");
  }
  // Small delay to prevent loop from running too fast
  // delay(10);
}
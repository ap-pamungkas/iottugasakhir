/*
 * ESP8266 LoRa SX1278 Receiver untuk Data GPS dan Sensor
 * 
 * Koneksi Pin:
 * SX1278    ESP8266 (NodeMCU)
 * VCC   --> 3.3V
 * GND   --> GND
 * MISO  --> D6 (GPIO12)
 * MOSI  --> D7  (GPIO13) 
 * SCK   --> D5 (GPIO14)
 * NSS   --> D8 (GPIO15)
 * RST   --> D0 (GPIO16)
 * DIO0  --> D1 (GPIO5)
 * 
 * Optional: LCD I2C Display
 * SDA   --> D2 (GPIO4)
 * SCL   --> D1 (GPIO5) - Bisa sharing dengan DIO0 jika tidak pakai interrupt
 */

#include <SPI.h>
#include <LoRa.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>

// Pin definitions untuk ESP8266
#define SS_PIN 15   // D8
#define RST_PIN 16  // D0
#define DIO0_PIN 5  // D1

// LED indicator (built-in LED)
#define LED_PIN 2   // D4 - Built-in LED

// Web server
ESP8266WebServer server(80);

// Variables untuk menyimpan data terakhir yang diterima
struct SensorData {
  String no_seri = "";
  float latitude = 0.0;
  float longitude = 0.0;
  float suhu = 0.0;
  int kualitas_udara = 0;
  String timestamp = "";
  int rssi = 0;
  float snr = 0.0;
  bool dataValid = false;
} lastData;

// Statistics
unsigned long totalPackets = 0;
unsigned long errorPackets = 0;
unsigned long apiSuccessCount = 0;
unsigned long apiErrorCount = 0;
unsigned long lastPacketTime = 0;

// WiFi credentials - WAJIB DIISI untuk mengirim ke API
const char* ssid = "YourWiFiSSID";     // Ganti dengan nama WiFi Anda
const char* password = "YourWiFiPassword"; // Ganti dengan password WiFi
bool wifiEnabled = true; // Set true untuk menggunakan WiFi dan API

// API Configuration
const char* apiURL = "http://localhost/sigap-io/api/insiden-log";
bool sendToAPI = true; // Set true untuk mengirim data ke API

void setup() {
  Serial.begin(115200);
  while (!Serial);
  
  // Setup LED
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH); // LED off (aktif LOW)
  
  Serial.println();
  Serial.println("=== ESP8266 LoRa Receiver untuk Data GPS dan Sensor ===");
  Serial.println("Fitur:");
  Serial.println("- Penerimaan data LoRa");
  Serial.println("- Tampilan data di Serial Monitor");
  Serial.println("- Web interface untuk monitoring");
  Serial.println("- POST data ke API: " + String(apiURL));
  Serial.println("======================================================");
  
  // Set pin LoRa
  LoRa.setPins(SS_PIN, RST_PIN, DIO0_PIN);
  
  // Inisialisasi LoRa dengan frekuensi yang sesuai dengan modul SX1278
  // Pilih frekuensi sesuai dengan modul Anda:
  // - Untuk SX1278 433MHz: gunakan 433E6 (433MHz)
  // - Untuk SX1278 868MHz: gunakan 868E6 atau 915E6
  
  long frequency = 433E6; // 433MHz - Ganti sesuai modul Anda
  
  if (!LoRa.begin(frequency)) {
    Serial.println("Starting LoRa failed!");
    Serial.println("Periksa:");
    Serial.println("1. Koneksi kabel");
    Serial.println("2. Frekuensi modul (433MHz atau 868MHz)");
    while (1) {
      digitalWrite(LED_PIN, LOW);
      delay(200);
      digitalWrite(LED_PIN, HIGH);
      delay(200);
    }
  }
  
  // Konfigurasi LoRa (harus sama dengan transmitter)
  LoRa.setSpreadingFactor(7);      // SF7
  LoRa.setSignalBandwidth(125E3);  // 125kHz bandwidth
  LoRa.setCodingRate4(5);          // 4/5 coding rate
  LoRa.setTxPower(20);             // Tx power 20dBm
  
  Serial.println("LoRa Receiver siap!");
  Serial.println("Frekuensi: " + String(frequency/1E6, 0) + "MHz");
  Serial.println("Spreading Factor: 7");
  Serial.println("Bandwidth: 125kHz");
  Serial.println("PENTING: Pastikan frekuensi sama dengan transmitter!");
  Serial.println("======================================================");
  
  // Setup WiFi (wajib untuk API)
  if (wifiEnabled) {
    setupWiFi();
  } else {
    Serial.println("⚠️  WiFi dinonaktifkan - tidak dapat mengirim ke API");
  }
  
  Serial.println("Menunggu data dari transmitter...");
  Serial.println("Data akan ditampilkan di Serial Monitor dan dikirim ke API");
  Serial.println();
}

void loop() {
  // Cek apakah ada paket LoRa yang diterima
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    handleLoRaPacket(packetSize);
  }
  
  // Handle web server jika WiFi enabled
  if (wifiEnabled) {
    server.handleClient();
  }
  
  // Blink LED jika ada data valid dalam 30 detik terakhir
  static unsigned long lastBlink = 0;
  if (lastData.dataValid && (millis() - lastPacketTime < 30000)) {
    if (millis() - lastBlink > 1000) {
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
      lastBlink = millis();
    }
  } else {
    digitalWrite(LED_PIN, HIGH); // LED off
  }
  
  // Tampilkan statistik setiap 60 detik
  static unsigned long lastStats = 0;
  if (millis() - lastStats > 60000) {
    showStatistics();
    lastStats = millis();
  }
  
  delay(10);
}

void handleLoRaPacket(int packetSize) {
  String receivedData = "";
  
  // Baca semua data dari paket
  while (LoRa.available()) {
    receivedData += (char)LoRa.read();
  }
  
  // Dapatkan RSSI dan SNR
  int rssi = LoRa.packetRssi();
  float snr = LoRa.packetSnr();
  
  totalPackets++;
  lastPacketTime = millis();
  
  Serial.println("======================================");
  Serial.println("📡 PAKET LORA DITERIMA");
  Serial.println("======================================");
  Serial.println("📊 Ukuran paket: " + String(packetSize) + " bytes");
  Serial.println("📶 RSSI: " + String(rssi) + " dBm");
  Serial.println("📡 SNR: " + String(snr) + " dB");
  Serial.println("💬 Data mentah: " + receivedData);
  Serial.println("======================================");
  
  // Coba parse sebagai JSON
  if (receivedData.startsWith("{") && receivedData.endsWith("}")) {
    parseJsonData(receivedData, rssi, snr);
  } else {
    Serial.println("📝 Tipe: Pesan teks biasa");
    Serial.println("======================================");
  }
  
  // Blink LED untuk indikasi paket diterima
  digitalWrite(LED_PIN, LOW);
  delay(100);
  digitalWrite(LED_PIN, HIGH);
}

void parseJsonData(String jsonData, int rssi, float snr) {
  // Buat JSON document
  DynamicJsonDocument doc(1024);
  
  // Parse JSON
  DeserializationError error = deserializeJson(doc, jsonData);
  
  if (error) {
    Serial.println("❌ Error parsing JSON: " + String(error.c_str()));
    errorPackets++;
    return;
  }
  
  Serial.println("✅ Tipe: Data JSON sensor berhasil diparsing");
  
  // Extract data
  if (doc.containsKey("no_seri")) {
    lastData.no_seri = doc["no_seri"].as<String>();
    lastData.latitude = doc["latitude"].as<float>();
    lastData.longitude = doc["longitude"].as<float>();
    lastData.suhu = doc["suhu"].as<float>();
    lastData.kualitas_udara = doc["kualitas_udara"].as<int>();
    lastData.rssi = rssi;
    lastData.snr = snr;
    lastData.timestamp = getCurrentTime();
    lastData.dataValid = true;
    
    // Tampilkan data yang diparsing dengan format yang lebih menarik
    Serial.println("");
    Serial.println("🔍 === DETAIL DATA SENSOR ===");
    Serial.println("📋 No Seri      : " + lastData.no_seri);
    Serial.println("📍 Koordinat    : " + String(lastData.latitude, 6) + ", " + String(lastData.longitude, 6));
    Serial.println("🌡️  Suhu         : " + String(lastData.suhu, 2) + "°C");
    Serial.println("💨 Kualitas Udara: " + String(lastData.kualitas_udara));
    Serial.println("🕐 Timestamp    : " + lastData.timestamp);
    Serial.println("📶 RSSI         : " + String(rssi) + " dBm");
    Serial.println("📡 SNR          : " + String(snr, 1) + " dB");
    
    // Analisis GPS
    Serial.println("");
    Serial.println("🛰️  === STATUS GPS ===");
    if (lastData.latitude != 0.0 && lastData.longitude != 0.0) {
      Serial.println("✅ GPS Status: VALID & AKTIF");
      Serial.println("🗺️  Lokasi dapat ditentukan");
    } else {
      Serial.println("❌ GPS Status: TIDAK VALID");
      Serial.println("⚠️  Koordinat menunjukkan 0.0, 0.0");
    }
    
    // Analisis kualitas sinyal
    Serial.println("");
    Serial.println("📡 === ANALISIS SINYAL ===");
    if (rssi > -70) {
      Serial.println("🟢 Kualitas Sinyal: SANGAT BAIK (> -70 dBm)");
    } else if (rssi > -85) {
      Serial.println("🔵 Kualitas Sinyal: BAIK (-70 to -85 dBm)");
    } else if (rssi > -100) {
      Serial.println("🟡 Kualitas Sinyal: SEDANG (-85 to -100 dBm)");
    } else {
      Serial.println("🔴 Kualitas Sinyal: LEMAH (< -100 dBm)");
    }
    
    // Analisis suhu
    Serial.println("");
    Serial.println("🌡️  === ANALISIS SUHU ===");
    if (lastData.suhu > 35) {
      Serial.println("🔥 Status: PANAS (> 35°C)");
    } else if (lastData.suhu > 25) {
      Serial.println("☀️  Status: NORMAL (25-35°C)");
    } else if (lastData.suhu > 15) {
      Serial.println("❄️  Status: SEJUK (15-25°C)");
    } else {
      Serial.println("🧊 Status: DINGIN (< 15°C)");
    }
    
    // Kirim ke API jika WiFi tersedia
    if (wifiEnabled && WiFi.status() == WL_CONNECTED && sendToAPI) {
      Serial.println("");
      Serial.println("🌐 === MENGIRIM KE API ===");
      sendDataToAPI();
    }
  }
  
  Serial.println("======================================");
  Serial.println("");
}

void setupWiFi() {
  Serial.println("Menghubungkan ke WiFi...");
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.println("WiFi terhubung!");
    Serial.println("IP Address: " + WiFi.localIP().toString());
    
    // Setup web server routes
    server.on("/", handleRoot);
    server.on("/data", handleData);
    server.on("/stats", handleStats);
    server.begin();
    
    Serial.println("Web server dimulai di http://" + WiFi.localIP().toString());
  } else {
    Serial.println();
    Serial.println("Gagal terhubung ke WiFi. Melanjutkan tanpa WiFi.");
    wifiEnabled = false;
  }
}

void handleRoot() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<title>LoRa Receiver Monitor</title>";
  html += "<meta http-equiv='refresh' content='5'>";
  html += "<style>body{font-family:Arial;margin:20px;}";
  html += ".container{max-width:800px;margin:0 auto;}";
  html += ".card{background:#f5f5f5;padding:15px;margin:10px 0;border-radius:5px;}";
  html += ".status{font-weight:bold;color:";
  html += (lastData.dataValid && (millis() - lastPacketTime < 30000)) ? "green" : "red";
  html += ";}</style></head><body>";
  
  html += "<div class='container'>";
  html += "<h1>LoRa Receiver Monitor</h1>";
  
  html += "<div class='card'>";
  html += "<h3>Status Receiver</h3>";
  html += "<p class='status'>Status: ";
  html += (lastData.dataValid && (millis() - lastPacketTime < 30000)) ? "AKTIF" : "TIDAK ADA DATA";
  html += "</p>";
  html += "<p>Total Paket: " + String(totalPackets) + "</p>";
  html += "<p>Error Parsing: " + String(errorPackets) + "</p>";
  html += "<p>API Berhasil: " + String(apiSuccessCount) + "</p>";
  html += "<p>API Gagal: " + String(apiErrorCount) + "</p>";
  html += "</div>";
  
  if (lastData.dataValid) {
    html += "<div class='card'>";
    html += "<h3>Data Terakhir</h3>";
    html += "<p><strong>No Seri:</strong> " + lastData.no_seri + "</p>";
    html += "<p><strong>Koordinat:</strong> " + String(lastData.latitude, 6) + ", " + String(lastData.longitude, 6) + "</p>";
    html += "<p><strong>Suhu:</strong> " + String(lastData.suhu, 2) + "°C</p>";
    html += "<p><strong>Kualitas Udara:</strong> " + String(lastData.kualitas_udara) + "</p>";
    html += "<p><strong>RSSI:</strong> " + String(lastData.rssi) + " dBm</p>";
    html += "<p><strong>SNR:</strong> " + String(lastData.snr, 1) + " dB</p>";
    html += "<p><strong>Waktu:</strong> " + lastData.timestamp + "</p>";
    html += "</div>";
  }
  
  html += "</div></body></html>";
  
  server.send(200, "text/html", html);
}

void handleData() {
  String json = "{";
  json += "\"status\":\"" + String((lastData.dataValid && (millis() - lastPacketTime < 30000)) ? "active" : "inactive") + "\",";
  json += "\"no_seri\":\"" + lastData.no_seri + "\",";
  json += "\"latitude\":" + String(lastData.latitude, 6) + ",";
  json += "\"longitude\":" + String(lastData.longitude, 6) + ",";
  json += "\"suhu\":" + String(lastData.suhu, 2) + ",";
  json += "\"kualitas_udara\":" + String(lastData.kualitas_udara) + ",";
  json += "\"rssi\":" + String(lastData.rssi) + ",";
  json += "\"snr\":" + String(lastData.snr, 1) + ",";
  json += "\"timestamp\":\"" + lastData.timestamp + "\",";
  json += "\"total_packets\":" + String(totalPackets) + ",";
  json += "\"error_packets\":" + String(errorPackets) + ",";
  json += "\"api_success\":" + String(apiSuccessCount) + ",";
  json += "\"api_errors\":" + String(apiErrorCount);
  json += "}";
  
  server.send(200, "application/json", json);
}

void handleStats() {
  String json = "{";
  json += "\"total_packets\":" + String(totalPackets) + ",";
  json += "\"error_packets\":" + String(errorPackets) + ",";
  json += "\"api_success\":" + String(apiSuccessCount) + ",";
  json += "\"api_errors\":" + String(apiErrorCount) + ",";
  json += "\"success_rate\":" + String(totalPackets > 0 ? ((totalPackets - errorPackets) * 100.0 / totalPackets) : 0, 1) + ",";
  json += "\"uptime\":" + String(millis() / 1000) + ",";
  json += "\"last_packet\":" + String((millis() - lastPacketTime) / 1000) + ",";
  json += "\"free_heap\":" + String(ESP.getFreeHeap());
  json += "}";
  
  server.send(200, "application/json", json);
}

void showStatistics() {
  Serial.println("📊 =====================================");
  Serial.println("📊 STATISTIK RECEIVER - " + getCurrentTime());
  Serial.println("📊 =====================================");
  Serial.println("📦 Total paket diterima  : " + String(totalPackets));
  Serial.println("❌ Error parsing JSON    : " + String(errorPackets));
  Serial.println("🌐 API berhasil          : " + String(apiSuccessCount));
  Serial.println("🚫 API gagal             : " + String(apiErrorCount));
  if (totalPackets > 0) {
    Serial.println("✅ Success rate parsing  : " + String((totalPackets - errorPackets) * 100.0 / totalPackets, 1) + "%");
  }
  if (apiSuccessCount + apiErrorCount > 0) {
    Serial.println("🌐 Success rate API     : " + String(apiSuccessCount * 100.0 / (apiSuccessCount + apiErrorCount), 1) + "%");
  }
  Serial.println("⏱️  Uptime               : " + String(millis() / 1000) + " detik");
  if (lastPacketTime > 0) {
    Serial.println("📡 Paket terakhir        : " + String((millis() - lastPacketTime) / 1000) + " detik yang lalu");
  }
  Serial.println("💾 Free heap            : " + String(ESP.getFreeHeap()) + " bytes");
  Serial.println("📶 WiFi Status          : " + String(WiFi.status() == WL_CONNECTED ? "Terhubung" : "Terputus"));
  Serial.println("📊 =====================================");
  Serial.println("");
}

String getCurrentTime() {
  // Simple timestamp based on uptime - bisa diganti dengan NTP untuk waktu real
  unsigned long seconds = millis() / 1000;
  unsigned long minutes = seconds / 60;
  unsigned long hours = minutes / 60;
  
  seconds = seconds % 60;
  minutes = minutes % 60;
  hours = hours % 24;
  
  String timeStr = "";
  if (hours < 10) timeStr += "0";
  timeStr += String(hours) + ":";
  if (minutes < 10) timeStr += "0";
  timeStr += String(minutes) + ":";
  if (seconds < 10) timeStr += "0";
  timeStr += String(seconds);
  
  return timeStr;
}

// Fungsi untuk mengirim data ke API
void sendDataToAPI() {
  if (!lastData.dataValid) {
    Serial.println("❌ Data tidak valid, tidak mengirim ke API");
    return;
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("❌ WiFi tidak terhubung, tidak dapat mengirim ke API");
    apiErrorCount++;
    return;
  }
  
  WiFiClient client;
  HTTPClient http;
  
  Serial.println("🌐 Memulai koneksi ke API...");
  Serial.println("🔗 URL: " + String(apiURL));
  
  http.begin(client, apiURL);
  http.addHeader("Content-Type", "application/json");
  
  // Buat JSON payload untuk API
  DynamicJsonDocument apiDoc(1024);
  apiDoc["no_seri"] = lastData.no_seri;
  apiDoc["latitude"] = lastData.latitude;
  apiDoc["longitude"] = lastData.longitude;
  apiDoc["suhu"] = lastData.suhu;
  apiDoc["kualitas_udara"] = lastData.kualitas_udara;
  apiDoc["rssi"] = lastData.rssi;
  apiDoc["snr"] = lastData.snr;
  apiDoc["timestamp"] = lastData.timestamp;
  
  // Tambahan metadata
  apiDoc["device_type"] = "ESP8266_LoRa";
  apiDoc["data_source"] = "LoRa_Receiver";
  
  String jsonPayload;
  serializeJson(apiDoc, jsonPayload);
  
  Serial.println("📤 JSON Payload:");
  Serial.println(jsonPayload);
  
  // Kirim POST request
  Serial.println("📡 Mengirim data ke server...");
  int httpResponseCode = http.POST(jsonPayload);
  
  // Handle response
  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println("✅ HTTP Response Code: " + String(httpResponseCode));
    Serial.println("📥 Server Response: " + response);
    
    if (httpResponseCode == 200 || httpResponseCode == 201) {
      Serial.println("🎉 Data berhasil dikirim ke API!");
      apiSuccessCount++;
    } else {
      Serial.println("⚠️  Data dikirim tapi server merespons dengan kode error");
      apiErrorCount++;
    }
  } else {
    Serial.println("❌ Error mengirim data ke API");
    Serial.println("💥 Error Code: " + String(httpResponseCode));
    Serial.println("🔍 Kemungkinan masalah:");
    Serial.println("   - Server tidak berjalan");
    Serial.println("   - URL API salah");
    Serial.println("   - Masalah koneksi internet");
    apiErrorCount++;
  }
  
  http.end();
  Serial.println("🔚 Koneksi API ditutup");
}

// Fungsi utilitas untuk debugging
void printHex(String data) {
  Serial.print("HEX: ");
  for (int i = 0; i < data.length(); i++) {
    if (data[i] < 16) Serial.print("0");
    Serial.print(String(data[i], HEX));
    Serial.print(" ");
  }
  Serial.println();
}

// Fungsi untuk mengubah konfigurasi receiver
void changeFrequency(long frequency) {
  Serial.println("Mengubah frekuensi ke: " + String(frequency));
  
  if (LoRa.begin(frequency)) {
    Serial.println("Frekuensi berhasil diubah!");
  } else {
    Serial.println("Gagal mengubah frekuensi!");
  }
}

void changeSpreadingFactor(int sf) {
  LoRa.setSpreadingFactor(sf);
  Serial.println("Spreading Factor diubah ke: SF" + String(sf));
}

void changeBandwidth(long bw) {
  LoRa.setSignalBandwidth(bw);
  Serial.println("Bandwidth diubah ke: " + String(bw) + " Hz");
}
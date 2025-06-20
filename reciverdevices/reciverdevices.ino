#include <SPI.h>
#include <LoRa.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

// ====================== Konfigurasi =======================
#define LORA_SS 15    // D8
#define LORA_RST 16   // D0
#define LORA_DIO0 5   // D1
#define BUZZER_PIN 4  // D2

const char *ssid = "Bujai";
const char *password = "qwertyui";

// Pastikan endpoint API mengarah langsung ke route yang menerima POST, bukan yang redirect
const String API_URL = "http://192.168.138.198/sigap-io_v_0.2/api/insiden-log";
// ==========================================================

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  WiFi.begin(ssid, password);
  Serial.print("Menghubungkan WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi terhubung!");

  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(433E6)) {
    Serial.println("Gagal memulai LoRa. Periksa koneksi!");
    while (true)
      ;
  }
  Serial.println("Receiver LoRa siap. Menunggu data...");
}

void loop() {
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String incoming = "";
    while (LoRa.available()) {
      incoming += (char)LoRa.read();
    }

    Serial.println("Data diterima:");
    Serial.println(incoming);

    bool isDarurat = incoming.startsWith("[DARURAT]");
    if (isDarurat) {
      Serial.println("!!! SINYAL DARURAT TERDETEKSI !!!");
      triggerBuzzer();
    }

    String noSeri;
    float lat = 0.0, lon = 0.0, suhu = 0.0;
    int kualitas = 0;

    if (parseData(incoming, noSeri, lat, lon, suhu, kualitas)) {
      kirimKeAPI(noSeri, lat, lon, suhu, kualitas, isDarurat);
    } else {
      Serial.println("Format data tidak valid. Lewatkan...");
    }

    Serial.println("--------------------------");
  }
}

// ===================== Parse Data =====================
bool parseData(String data, String &noSeri, float &lat, float &lon, float &suhu, int &kualitas) {
  int idStart = data.indexOf("[ID:");
  int idEnd = data.indexOf("]", idStart);
  if (idStart != -1 && idEnd != -1) {
    noSeri = data.substring(idStart + 4, idEnd);
  } else {
    noSeri = "UNKNOWN";
  }

  // Cari posisi label
  int locIndex = data.indexOf("Lokasi:");
  int latIndex = data.indexOf("Lat:");
  int lonIndex = data.indexOf("Lon:");
  int mqIndex = data.indexOf("MQ135:");
  int suhuIndex = data.indexOf("Suhu:");

  if ((locIndex != -1 && mqIndex != -1 && suhuIndex != -1) || (latIndex != -1 && lonIndex != -1 && mqIndex != -1 && suhuIndex != -1)) {

    if (locIndex != -1) {  // Format darurat
      String locStr = data.substring(locIndex + 7, mqIndex);
      locStr.trim();
      int comma = locStr.indexOf(',');
      if (comma > 0) {
        lat = locStr.substring(0, comma).toFloat();
        lon = locStr.substring(comma + 1).toFloat();
      }
    } else {  // Format biasa
      lat = data.substring(latIndex + 4, data.indexOf(",", latIndex)).toFloat();
      lon = data.substring(lonIndex + 4, data.indexOf(",", lonIndex)).toFloat();
    }

    kualitas = data.substring(mqIndex + 7, data.indexOf(",", mqIndex)).toInt();
    suhu = data.substring(suhuIndex + 6, data.indexOf("C", suhuIndex)).toFloat();

    // Cek NaN
    if (isnan(lat)) lat = -1.1010101;
    if (isnan(lon)) lon = 9.28228282;
    if (isnan(suhu)) suhu = 0.0;

    return true;
  }

  return false;
}

// ==================== Buzzer =====================
void triggerBuzzer() {
  for (int i = 0; i < 3; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(300);
    digitalWrite(BUZZER_PIN, LOW);
    delay(300);
  }
}

// ==================== Kirim ke API =====================
void kirimKeAPI(String noSeri, float lat, float lon, float suhu, int kualitas, bool darurat) {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClient client;
    HTTPClient http;

    http.begin(client, API_URL);  // ✅ Gunakan versi baru
    http.addHeader("Content-Type", "application/json");

    String payload = "{";
    payload += "\"no_seri\":\"" + noSeri + "\",";
    payload += "\"latitude\":" + String(lat, 6) + ",";
    payload += "\"longitude\":" + String(lon, 6) + ",";
    payload += "\"suhu\":" + String(suhu, 2) + ",";
    payload += "\"kualitas_udara\":" + String(kualitas) + ",";
    payload += "\"darurat\":" + String(darurat ? "true" : "false");
    payload += "}";

    Serial.println("Payload:");
    Serial.println(payload);

    int httpCode = http.POST(payload);
    String response = http.getString();

    Serial.print("HTTP Status: ");
    Serial.println(httpCode);
    Serial.print("Response: ");
    Serial.println(response);

    http.end();
  } else {
    Serial.println("WiFi tidak terhubung, tidak bisa kirim data.");
  }
}

#include <SPI.h>
#include <LoRa.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

// ====================== Konfigurasi =======================
#define LORA_SS    15  // D8
#define LORA_RST   16  // D0
#define LORA_DIO0  5   // D1

const char* ssid     = "NAMA_WIFI";      // Ganti dengan SSID WiFi kamu
const char* password = "PASSWORD_WIFI";  // Ganti dengan password WiFi

const String API_URL = "http://namadomain/api/insiden-log";  // Ganti dengan domain kamu
// =========================================================

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Koneksi WiFi
  WiFi.begin(ssid, password);
  Serial.print("Menghubungkan WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi terhubung!");

  // Inisialisasi LoRa
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(433E6)) {
    Serial.println("Gagal memulai LoRa. Periksa koneksi!");
    while (true);
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

    // Parsing data
    float lat = 0.0, lon = 0.0, suhu = 0.0;
    int kualitas = 0;
    bool valid = parseData(incoming, lat, lon, suhu, kualitas);

    if (valid) {
      kirimKeAPI(lat, lon, suhu, kualitas);
    } else {
      Serial.println("Format data tidak valid. Lewatkan...");
    }

    Serial.println("--------------------------");
  }
}

// ===================== Fungsi Parse Data =====================
bool parseData(String data, float &lat, float &lon, float &suhu, int &kualitas) {
  // Contoh format: Lat: -1.8305, Lon: 110.5323, MQ135: 140, Suhu: 25.4 C, Tekanan: 1012.56 hPa

  int i1 = data.indexOf("Lat:");
  int i2 = data.indexOf("Lon:");
  int i3 = data.indexOf("MQ135:");
  int i4 = data.indexOf("Suhu:");

  if (i1 == -1 || i2 == -1 || i3 == -1 || i4 == -1) {
    return false;
  }

  lat = data.substring(i1 + 5, data.indexOf(",", i1)).toFloat();
  lon = data.substring(i2 + 5, data.indexOf(",", i2)).toFloat();
  kualitas = data.substring(i3 + 7, data.indexOf(",", i3)).toInt();
  suhu = data.substring(i4 + 6, data.indexOf("C", i4)).toFloat();

  return true;
}

// ==================== Kirim ke API =====================
void kirimKeAPI(float lat, float lon, float suhu, int kualitas) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(API_URL);
    http.addHeader("Content-Type", "application/json");

    String payload = "{";
    payload += "\"no_seri\":\"" + NO_SERI + "\",";
    payload += "\"latitude\":" + String(lat, 6) + ",";
    payload += "\"longitude\":" + String(lon, 6) + ",";
    payload += "\"suhu\":" + String(suhu, 2) + ",";
    payload += "\"kualitas_udara\":" + String(kualitas);
    payload += "}";

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

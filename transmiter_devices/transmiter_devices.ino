/*
 * ESP8266 LoRa SX1278 Transmitter with GPS
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
 * GPS NEO-6M/8M:
 * VCC   --> 3.3V
 * GND   --> GND
 * TX    --> D2 (GPIO4)
 * RX    --> D3 (GPIO0) - Optional untuk konfigurasi
 * 
 * BMP280
 * SDA -> D2
 * SCL -> D1
 */

#include <SPI.h>
#include <LoRa.h>
#include <SoftwareSerial.h>
#include <TinyGPS++.h>
#include <Adafruit_BMP280.h>


// Pin definitions untuk ESP8266
#define SS_PIN 15   // D8
#define RST_PIN 16  // D0
#define DIO0_PIN 5  // D1

#define MQ135_PIN A0  // pin analog A0


// Pin definitions untuk GPS
#define GPS_RX_PIN 4  // D2 - GPS TX ke ESP8266 RX
#define GPS_TX_PIN 0  // D3 - GPS RX ke ESP8266 TX (optional)

// Inisialisasi GPS
SoftwareSerial ss(GPS_RX_PIN, GPS_TX_PIN);
TinyGPSPlus gps;
Adafruit_BMP280 bmp;  // Menggunakan I2C
float bmp_temp = 0.0;

// Counter untuk pesan
int counter = 0;
int mq135_value = 0;
// GPS variables
double latitude = 0.0;
double longitude = 0.0;
double altitude = 0.0;
int satellites = 0;
String gpsTime = "";
String gpsDate = "";
bool gpsValid = false;

void setup() {
  Serial.begin(115200);
  ss.begin(9600);  // GPS baud rate
  while (!Serial)
    ;

  Serial.println("ESP8266 LoRa Transmitter with GPS");

  // Set pin LoRa
  LoRa.setPins(SS_PIN, RST_PIN, DIO0_PIN);

  // Inisialisasi LoRa dengan frekuensi 915MHz (sesuaikan dengan regulasi di Indonesia)
  // Untuk Indonesia bisa gunakan 920MHz - 925MHz
  if (!LoRa.begin(920E6)) {
    Serial.println("Starting LoRa failed!");
    while (1)
      ;
  }

  if (!bmp.begin(0x76)) {  // Ganti 0x76 dengan 0x77 jika sensor kamu pakai alamat itu
    Serial.println("BMP280 tidak terdeteksi!");
  } else {
    Serial.println("BMP280 berhasil diinisialisasi.");
  }


  // Konfigurasi LoRa
  LoRa.setSpreadingFactor(7);      // SF7-SF12, semakin tinggi semakin jauh tapi lambat
  LoRa.setSignalBandwidth(125E3);  // 125kHz bandwidth
  LoRa.setCodingRate4(5);          // 4/5 coding rate
  LoRa.setTxPower(20);             // Tx power 20dBm (maksimum)

  Serial.println("LoRa Transmitter siap!");
  Serial.println("GPS initializing...");
  Serial.println("Frekuensi: 920MHz");
  Serial.println("Spreading Factor: 7");
  Serial.println("Bandwidth: 125kHz");
  Serial.println("Tx Power: 20dBm");
  Serial.println("------------------------");
  Serial.println("Menunggu sinyal GPS...");
}

void loop() {
  // Baca data GPS
  readGPS();

  Serial.print("Mengirim paket: ");
  Serial.println(counter);

  // Kirim paket basic
  LoRa.beginPacket();
  LoRa.print("Hello LoRa #");
  LoRa.print(counter);
  LoRa.print(" dari ESP8266");
  LoRa.endPacket();

  // Kirim data sensor + GPS
  sendSensorAndGPSData();

  counter++;

  // Tunggu 10 detik sebelum kirim lagi
  delay(10000);
}

void sendSensorAndGPSData() {
  // Contoh data sensor (simulasi) - Anda bisa mengganti ini dengan pembacaan sensor sebenarnya
  // Untuk kesederhanaan, saya akan gunakan nilai yang sudah ada atau simulasi
  float temperature = 25.5 + random(-50, 50) / 10.0; // Ini contoh, sesuaikan jika Anda memiliki sensor suhu lain
  mq135_value = analogRead(MQ135_PIN);              // Baca data dari MQ135
  bmp_temp = bmp.readTemperature();                 // Baca suhu dari BMP280 (Satuan: °C)

  // Pastikan bmp_temp valid, jika tidak, set ke nilai default
  if (isnan(bmp_temp)) {
    bmp_temp = 0.0;
  }

  // Gunakan bmp_temp sebagai nilai suhu utama jika BMP280 digunakan
  float final_temperature = bmp_temp;

  Serial.println("Mengirim data sensor + GPS:");
  Serial.println("BMP280 Temp: " + String(final_temperature) + "°C");
  Serial.println("MQ135 (Analog): " + String(mq135_value));

  if (gpsValid) {
    Serial.println("GPS Valid - Koordinat:");
    Serial.println("Lat: " + String(latitude, 6));
    Serial.println("Lng: " + String(longitude, 6));
    Serial.println("Alt: " + String(altitude, 2) + "m");
    Serial.println("Satelit: " + String(satellites));
    Serial.println("Waktu: " + gpsTime);
    Serial.println("Tanggal: " + gpsDate);
  } else {
    Serial.println("GPS: Tidak ada sinyal");
  }

  // Format data dalam JSON sesuai permintaan
  String sensorData = "{";
  sensorData += "\"no_seri\":\"SN-PRK-002\",";

  if (gpsValid) {
    sensorData += "\"latitude\":" + String(latitude, 6) + ",";
    sensorData += "\"longitude\":" + String(longitude, 6) + ",";
  } else {
    // Default values if GPS is not valid
    sensorData += "\"latitude\":0.0,";
    sensorData += "\"longitude\":0.0,";
  }

  sensorData += "\"suhu\":" + String(final_temperature, 2) + ","; // Menggunakan 2 angka di belakang koma untuk suhu
  sensorData += "\"kualitas_udara\":" + String(mq135_value);
  sensorData += "}";

  // Kirim data sensor + GPS
  LoRa.beginPacket();
  LoRa.print(sensorData);
  LoRa.endPacket();

  Serial.println("Data terkirim: " + sensorData);
  Serial.println("------------------------");
}
// Fungsi untuk membaca data GPS
void readGPS() {
  static unsigned long lastGPSCheck = 0;
  static bool dataReceived = false;

  while (ss.available() > 0) {
    char c = ss.read();
    dataReceived = true;

    // Debug: tampilkan raw data GPS (uncomment jika perlu)
    // Serial.print(c);

    if (gps.encode(c)) {
      if (gps.location.isValid()) {
        latitude = gps.location.lat();
        longitude = gps.location.lng();
        gpsValid = true;

        // Print GPS fix info
        static unsigned long lastFixPrint = 0;
        if (millis() - lastFixPrint > 5000) {
          Serial.println("*** GPS FIX ACQUIRED! ***");
          Serial.println("Lat: " + String(latitude, 6));
          Serial.println("Lng: " + String(longitude, 6));
          lastFixPrint = millis();
        }
      }

      if (gps.altitude.isValid()) {
        altitude = gps.altitude.meters();
      }

      if (gps.satellites.isValid()) {
        satellites = gps.satellites.value();
      }

      if (gps.time.isValid()) {
        // Format waktu HH:MM:SS (UTC)
        gpsTime = "";
        if (gps.time.hour() < 10) gpsTime += "0";
        gpsTime += String(gps.time.hour()) + ":";
        if (gps.time.minute() < 10) gpsTime += "0";
        gpsTime += String(gps.time.minute()) + ":";
        if (gps.time.second() < 10) gpsTime += "0";
        gpsTime += String(gps.time.second());
      }

      if (gps.date.isValid()) {
        // Format tanggal DD/MM/YYYY
        gpsDate = "";
        if (gps.date.day() < 10) gpsDate += "0";
        gpsDate += String(gps.date.day()) + "/";
        if (gps.date.month() < 10) gpsDate += "0";
        gpsDate += String(gps.date.month()) + "/";
        gpsDate += String(gps.date.year());
      }
    }
  }

  // Status GPS setiap 10 detik
  if (millis() - lastGPSCheck > 10000) {
    lastGPSCheck = millis();

    Serial.println("=== STATUS GPS ===");
    Serial.println("Data diterima dari GPS: " + String(dataReceived ? "YA" : "TIDAK"));
    Serial.println("Characters processed: " + String(gps.charsProcessed()));
    Serial.println("Sentences passed: " + String(gps.passedChecksum()));
    Serial.println("Sentences failed: " + String(gps.failedChecksum()));

    if (gps.satellites.isValid()) {
      Serial.println("Satelit terdeteksi: " + String(gps.satellites.value()));
    } else {
      Serial.println("Satelit: Tidak ada data");
    }

    if (gps.location.isValid()) {
      Serial.println("GPS Status: FIXED");
      gpsValid = true;
    } else {
      Serial.println("GPS Status: SEARCHING...");
      gpsValid = false;
    }

    if (gps.hdop.isValid()) {
      Serial.println("HDOP (akurasi): " + String(gps.hdop.hdop()));
    }

    Serial.println("==================");

    // Reset flag
    dataReceived = false;
  }

  // Check jika GPS tidak mengirim data sama sekali
  if (millis() > 30000 && gps.charsProcessed() < 10) {
    static unsigned long lastWarning = 0;
    if (millis() - lastWarning > 15000) {
      Serial.println("⚠️  PERINGATAN: GPS tidak mengirim data!");
      Serial.println("   Periksa koneksi kabel:");
      Serial.println("   - GPS TX -> ESP8266 D2 (GPIO4)");
      Serial.println("   - GPS VCC -> 3.3V");
      Serial.println("   - GPS GND -> GND");
      lastWarning = millis();
    }
  }
}

// Fungsi untuk mengirim hanya data GPS
void sendGPSOnly() {
  if (!gpsValid) {
    Serial.println("GPS tidak valid, tidak mengirim data");
    return;
  }

  String gpsData = "{";
  gpsData += "\"type\":\"gps_only\",";
  gpsData += "\"id\":\"ESP8266_01\",";
  gpsData += "\"lat\":" + String(latitude, 6) + ",";
  gpsData += "\"lng\":" + String(longitude, 6) + ",";
  gpsData += "\"alt\":" + String(altitude, 2) + ",";
  gpsData += "\"sat\":" + String(satellites) + ",";
  gpsData += "\"time\":\"" + gpsTime + "\",";
  gpsData += "\"date\":\"" + gpsDate + "\"";
  gpsData += "}";

  LoRa.beginPacket();
  LoRa.print(gpsData);
  LoRa.endPacket();

  Serial.println("GPS Data terkirim: " + gpsData);
}

// Fungsi untuk mengirim pesan custom
void sendCustomMessage(String message) {
  Serial.println("Mengirim pesan: " + message);

  LoRa.beginPacket();
  LoRa.print(message);
  LoRa.endPacket();

  Serial.println("Pesan terkirim!");
}

// Fungsi untuk mengubah frekuensi
void changeFrequency(long frequency) {
  Serial.println("Mengubah frekuensi ke: " + String(frequency));

  if (LoRa.begin(frequency)) {
    Serial.println("Frekuensi berhasil diubah!");
  } else {
    Serial.println("Gagal mengubah frekuensi!");
  }
}

// Fungsi untuk mengubah Tx Power
void changeTxPower(int power) {
  LoRa.setTxPower(power);
  Serial.println("Tx Power diubah ke: " + String(power) + "dBm");
}

// Fungsi untuk mengubah Spreading Factor
void changeSpreadingFactor(int sf) {
  LoRa.setSpreadingFactor(sf);
  Serial.println("Spreading Factor diubah ke: SF" + String(sf));
}
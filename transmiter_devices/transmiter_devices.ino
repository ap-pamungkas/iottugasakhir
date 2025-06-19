#include <SPI.h>
#include <LoRa.h>
#include <TinyGPS++.h>
#include <SoftwareSerial.h>
#include <Wire.h>
#include <Adafruit_BMP280.h>

// ======================== PIN KONFIGURASI ========================
// LoRa SX1278
#define LORA_SS   15  // D8
#define LORA_RST  16  // D0
#define LORA_DIO0 5   // D1

// GPS (TX GPS ke RX ESP8266)
#define GPS_RX 4     // D2 (GPIO4)
#define GPS_TX -1    // tidak digunakan

// Sensor
#define MQ135_PIN A0

// Tombol Darurat (aktif LOW, menggunakan GPIO3 / RX)
#define BUTTON_PIN 3

// ================================================================

TinyGPSPlus gps;
SoftwareSerial gpsSerial(GPS_RX, GPS_TX); // RX, TX (-1 tidak digunakan)
Adafruit_BMP280 bmp;

unsigned long lastPrint = 0;
unsigned long lastLoRaSend = 0;
const long LORA_SEND_INTERVAL = 10000;

void setup() {
  delay(1000);
  Serial.begin(115200);
  gpsSerial.begin(9600);  // Baud rate GPS NEO-6M

  Wire.begin(0, 2); // I2C SDA = GPIO0 (D3), SCL = GPIO2 (D4)

  pinMode(BUTTON_PIN, INPUT_PULLUP); // Tombol darurat

  // Inisialisasi BMP280
  if (!bmp.begin(0x76)) {
    Serial.println("Gagal mendeteksi BMP280!");
  } else {
    Serial.println("BMP280 siap!");
  }

  // Inisialisasi LoRa
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(433E6)) {
    Serial.println("LoRa init gagal!");
    while (1);
  }
  Serial.println("LoRa siap!");
}

void loop() {
  // Baca data GPS
  while (gpsSerial.available() > 0) {
    gps.encode(gpsSerial.read());
  }

  // Tampilkan info setiap 5 detik
  if (millis() - lastPrint > 5000) {
    tampilkanStatus();
    lastPrint = millis();
  }

  // Kirim data biasa tiap interval
  if (gps.location.isUpdated() || (millis() - lastLoRaSend > LORA_SEND_INTERVAL)) {
    kirimDataBiasa();
    lastLoRaSend = millis();
  }

  // Cek tombol darurat
  if (digitalRead(BUTTON_PIN) == LOW) {
    kirimDarurat();
    delay(3000);  // Hindari pengiriman berulang
  }
}

void tampilkanStatus() {
  if (!gps.location.isValid()) {
    Serial.println("GPS belum valid! Menunggu sinyal...");
  } else {
    Serial.print("Satelit: ");
    Serial.print(gps.satellites.value());
    Serial.print(" | HDOP: ");
    Serial.print(gps.hdop.hdop());
  }

  int mq135Value = analogRead(MQ135_PIN);
  float temperature = bmp.readTemperature();
  float pressure = bmp.readPressure() / 100.0F;

  Serial.print(" | MQ135: ");
  Serial.print(mq135Value);
  Serial.print(" | Suhu: ");
  Serial.print(temperature);
  Serial.print(" C | Tekanan: ");
  Serial.print(pressure);
  Serial.println(" hPa");
}

void kirimDataBiasa() {
  String data;

  if (gps.location.isValid()) {
    data = "Lat: " + String(gps.location.lat(), 6) +
           ", Lon: " + String(gps.location.lng(), 6);
  } else {
    data = "Lat: INVALID, Lon: INVALID";
  }

  int mq135Value = analogRead(MQ135_PIN);
  float temperature = bmp.readTemperature();
  float pressure = bmp.readPressure() / 100.0F;

  data += ", MQ135: " + String(mq135Value);
  data += ", Suhu: " + String(temperature, 2) + " C";
  data += ", Tekanan: " + String(pressure, 2) + " hPa";

  sendLoRa(data);
  Serial.print("Terkirim via LoRa: ");
  Serial.println(data);
}

void kirimDarurat() {
  String pesan = "[DARURAT] ";

  if (gps.location.isValid()) {
    pesan += "Lokasi: " + String(gps.location.lat(), 6) + "," + String(gps.location.lng(), 6);
  } else {
    pesan += "Lokasi: INVALID";
  }

  int mq135Value = analogRead(MQ135_PIN);
  float temperature = bmp.readTemperature();
  float pressure = bmp.readPressure() / 100.0F;

  pesan += ", MQ135: " + String(mq135Value);
  pesan += ", Suhu: " + String(temperature, 2) + " C";
  pesan += ", Tekanan: " + String(pressure, 2) + " hPa";

  sendLoRa(pesan);
  Serial.println(">>> SINYAL DARURAT TERKIRIM <<<");
  Serial.println(pesan);
}

void sendLoRa(String message) {
  LoRa.beginPacket();
  LoRa.print(message);
  LoRa.endPacket();
}

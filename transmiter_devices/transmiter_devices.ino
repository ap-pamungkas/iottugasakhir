#include <SPI.h>
#include <LoRa.h>
#include <TinyGPS++.h>
#include <SoftwareSerial.h>
#include <Wire.h>
#include <Adafruit_BMP280.h>  // Library BMP280

// Konfigurasi pin LoRa SX1278
#define LORA_SS   15  // D8
#define LORA_RST  16  // D0
#define LORA_DIO0 5   // D1

// Konfigurasi pin GPS (GPS TX ke pin ini)
#define GPS_RX 4      // D2 (GPIO4) - Aman untuk SoftwareSerial RX
#define GPS_TX -1     // Tidak digunakan, karena GPS hanya TX

// Konfigurasi pin Sensor MQ135
#define MQ135_PIN A0

TinyGPSPlus gps;
SoftwareSerial gpsSerial(GPS_RX, GPS_TX);  // RX, TX (-1 artinya tidak dipakai)
Adafruit_BMP280 bmp;  // Inisialisasi BMP280

unsigned long lastPrint = 0;
unsigned long lastLoRaSend = 0;
const long LORA_SEND_INTERVAL = 10000; // Kirim data LoRa setiap 10 detik

void setup() {
  delay(1000);  // Tunggu boot selesai
  Serial.begin(115200);
  gpsSerial.begin(9600);  // Baud rate untuk NEO-6M

  // Inisialisasi I2C (gunakan GPIO0 dan GPIO2)
  Wire.begin(0, 2); // SDA = GPIO0 (D3), SCL = GPIO2 (D4)

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
  // Membaca data GPS secara terus-menerus
  while (gpsSerial.available() > 0) {
    gps.encode(gpsSerial.read());
  }

  // Setiap 5 detik tampilkan status sinyal GPS dan nilai sensor
  if (millis() - lastPrint > 5000) {
    if (!gps.location.isValid()) {
      Serial.println("GPS belum valid! Menunggu sinyal...");
    } else {
      Serial.print("Satelit: ");
      Serial.print(gps.satellites.value());
      Serial.print(" | HDOP: ");
      Serial.print(gps.hdop.hdop());
    }

    // Baca nilai sensor MQ135
    int mq135Value = analogRead(MQ135_PIN);
    Serial.print(" | MQ135 (ADC): ");
    Serial.print(mq135Value);

    // Baca suhu dan tekanan dari BMP280
    float temperature = bmp.readTemperature();
    float pressure = bmp.readPressure() / 100.0F;

    Serial.print(" | Suhu: ");
    Serial.print(temperature);
    Serial.print(" C | Tekanan: ");
    Serial.print(pressure);
    Serial.println(" hPa");

    lastPrint = millis();
  }

  // Kirim data melalui LoRa secara berkala
  if (gps.location.isUpdated() || (millis() - lastLoRaSend > LORA_SEND_INTERVAL)) {
    String data;
    if (gps.location.isValid()) {
      data = "Lat: " + String(gps.location.lat(), 6) +
             ", Lon: " + String(gps.location.lng(), 6);
    } else {
      data = "Lat: INVALID, Lon: INVALID";
    }

    // Tambahkan data sensor
    int mq135Value = analogRead(MQ135_PIN);
    float temperature = bmp.readTemperature();
    float pressure = bmp.readPressure() / 100.0F;

    data += ", MQ135: " + String(mq135Value);
    data += ", Suhu: " + String(temperature, 2) + " C";
    data += ", Tekanan: " + String(pressure, 2) + " hPa";

    sendLoRa(data);
    Serial.print("Terkirim via LoRa: ");
    Serial.println(data);
    lastLoRaSend = millis();
  }
}

void sendLoRa(String message) {
  LoRa.beginPacket();
  LoRa.print(message);
  LoRa.endPacket();
}

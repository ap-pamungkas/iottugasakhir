#include <SPI.h>
#include <LoRa.h>
#include <SoftwareSerial.h>
#include <TinyGPS++.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_Sensor.h>

// ========================== PIN CONFIG ==========================
#define LORA_SS   15  // D8
#define LORA_RST  16  // D0
#define LORA_DIO0 5   // D1

#define MQ135_PIN A0 // analog pin

#define GPS_RX_PIN 0  // D3 - GPS TX ke sini
#define GPS_TX_PIN 2  // D4 - optional

Adafruit_BMP280 bmp;
SoftwareSerial gpsSerial(GPS_RX_PIN, GPS_TX_PIN);
TinyGPSPlus gps;

float bmp_temp = 0.0;
int mq135_value = 0;
int counter = 0;

double latitude = 0.0;
double longitude = 0.0;
double altitude = 0.0;
int satellites = 0;
String gpsTime = "";
String gpsDate = "";
bool gpsValid = false;

// ========================== SETUP ==========================
void setup() {
  Serial.begin(115200);
  gpsSerial.begin(9600);
  delay(100);

  Serial.println("=== ESP8266 Transmitter ===");

  // BMP280
  if (!bmp.begin(0x76)) {
    Serial.println("BMP280 gagal!");
  } else {
    Serial.println("BMP280 siap.");
  }

  // LoRa
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(433E6)) {
    Serial.println("LoRa gagal mulai!");
    while (1);
  }

  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(5);
  LoRa.setTxPower(20);
  Serial.println("LoRa siap kirim!");
}

// ========================== LOOP ==========================
void loop() {
  bacaGPS();
  kirimData();
  delay(10000); // kirim tiap 10 detik
}

// ========================== GPS ==========================
void bacaGPS() {
  while (gpsSerial.available() > 0) {
    char c = gpsSerial.read();
    gps.encode(c);

    if (gps.location.isValid()) {
      gpsValid = true;
      latitude = gps.location.lat();
      longitude = gps.location.lng();
    } else {
      gpsValid = false;
    }

    if (gps.altitude.isValid()) {
      altitude = gps.altitude.meters();
    }

    if (gps.satellites.isValid()) {
      satellites = gps.satellites.value();
    }

    if (gps.time.isValid()) {
      gpsTime = String(gps.time.hour()) + ":" +
                String(gps.time.minute()) + ":" +
                String(gps.time.second());
    }

    if (gps.date.isValid()) {
      gpsDate = String(gps.date.day()) + "/" +
                String(gps.date.month()) + "/" +
                String(gps.date.year());
    }
  }
}

// ========================== KIRIM DATA ==========================
void kirimData() {
  mq135_value = analogRead(MQ135_PIN);
  bmp_temp = bmp.readTemperature();
  if (isnan(bmp_temp)) bmp_temp = 0.0;

  String json = "{";
  json += "\"no_seri\":\"SN-PRK-002\",";
  json += "\"latitude\":" + String(gpsValid ? latitude : 0.0, 6) + ",";
  json += "\"longitude\":" + String(gpsValid ? longitude : 0.0, 6) + ",";
  json += "\"suhu\":" + String(bmp_temp, 2) + ",";
  json += "\"kualitas_udara\":" + String(mq135_value);
  json += "}";

  Serial.println("Mengirim data:");
  Serial.println(json);

  LoRa.beginPacket();
  LoRa.print(json);
  LoRa.endPacket();
}

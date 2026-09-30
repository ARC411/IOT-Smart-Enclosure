#include <Arduino.h>
#include <DHT.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Kredensial Wi-Fi bawaan Wokwi
const char *ssid = "Wokwi-GUEST";
const char *password = "";

// Ganti dengan URL Ngrok kamu yang BARU (awalan http://) + /api/telemetry
const char *serverUrl = "http://shawl-rejoicing-chug.ngrok-free.dev/api/telemetry";

// =====================
// PIN
// =====================
#define DHT_PIN 4
#define DHT_TYPE DHT22

#define MQ_PIN 34
#define RELAY_PIN 26
#define SERVO_PIN 25

// OLED I2C
#define OLED_SDA 21
#define OLED_SCL 22

// =====================
// OBJECT
// =====================
DHT dht(DHT_PIN, DHT_TYPE);
Servo ventilationServo;

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// =====================
// THRESHOLD
// =====================
float TEMP_THRESHOLD = 30.0;
float HUMID_THRESHOLD = 70.0;
int GAS_THRESHOLD = 2000;

// =====================
// SETUP
// =====================
void setup()
{
  Serial.begin(115200);

  // DHT
  dht.begin();

  // Relay
  pinMode(RELAY_PIN, OUTPUT);

  // Servo
  ventilationServo.attach(SERVO_PIN);

  // Kondisi awal
  digitalWrite(RELAY_PIN, LOW);
  ventilationServo.write(0);

  // OLED
  Wire.begin(OLED_SDA, OLED_SCL);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  {
    Serial.println("OLED gagal!");
    while (true);
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("SMART ENCLOSURE");
  display.println("System Starting...");

  display.display();
  delay(2000);

  WiFi.begin(ssid, password);
  Serial.print("Menghubungkan ke WiFi");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Terhubung!");
}

// =====================
// LOOP
// =====================
void loop()
{
  // 1. Baca sensor
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  int gasValue = analogRead(MQ_PIN);

  // Cek DHT
  if (isnan(temperature) || isnan(humidity))
  {
    Serial.println("Gagal membaca DHT22!");
    delay(1000);
    return;
  }

  // 2. Hitung logika otomatis (TAPI JANGAN DIEKSEKUSI DULU)
  bool temperatureHigh = temperature > TEMP_THRESHOLD;
  bool gasHigh = gasValue > GAS_THRESHOLD;
  bool humidHigh = humidity > HUMID_THRESHOLD; 

  bool autoVentilationNeeded = temperatureHigh || gasHigh || humidHigh;

  // 3. Buat variabel state akhir yang akan memegang perintah mutlak
  bool finalFanState = autoVentilationNeeded;
  bool finalVentState = autoVentilationNeeded;

  // 4. HTTP POST & MINTA IZIN KE LARAVEL
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverUrl); 
    http.addHeader("Content-Type", "application/json");
    http.addHeader("ngrok-skip-browser-warning", "1"); 

    // Bungkus data telemetri yang akan dilaporkan
    StaticJsonDocument<200> doc;
    doc["temperature"] = temperature;
    doc["humidity"] = humidity;
    doc["gas_ppm"] = gasValue;
    
    // Kita laporkan state otomatis ke database
    doc["fan_status"] = autoVentilationNeeded ? 1 : 0;
    doc["vent_status"] = autoVentilationNeeded ? 1 : 0;

    String jsonString;
    serializeJson(doc, jsonString);

    // Kirim data dan tunggu balasan
    int httpResponseCode = http.POST(jsonString);
    
    if (httpResponseCode > 0) {
      // BACA BALASAN DARI LARAVEL!
      String payload = http.getString();
      
      StaticJsonDocument<256> respDoc;
      deserializeJson(respDoc, payload);
      
      String mode = respDoc["mode"] | "auto";
      
      // PERBAIKAN: Gunakan tipe bool (boolean), bukan int!
      bool manualFan = respDoc["fan"] | false;
      bool manualVent = respDoc["vent"] | false;

      // JIKA LARAVEL BILANG "MANUAL", MAKA LOGIKA OTOMATIS DIBUANG!
      if (mode == "manual") {
          finalFanState = manualFan;
          finalVentState = manualVent;
      }
    } else {
      Serial.print("Gagal konek ke Laravel: ");
      Serial.println(httpResponseCode);
      // Jika internet mati, dia akan pakai autoVentilationNeeded sebagai cadangan
    }
    http.end();
  }

  // 5. EKSEKUSI AKTUATOR SEKARANG (SUDAH MUTLAK)
  digitalWrite(RELAY_PIN, finalFanState ? HIGH : LOW);
  
  // PERBAIKAN: Gunakan finalVentState, bukan finalFanState!
  ventilationServo.write(finalVentState ? 90 : 0);

  // =====================
  // SERIAL MONITOR & OLED
  // =====================
  Serial.println("====================");
  Serial.print("Temperature : "); Serial.print(temperature); Serial.println(" C");
  Serial.print("Humidity    : "); Serial.print(humidity); Serial.println(" %");
  Serial.print("Gas         : "); Serial.println(gasValue);
  
  // Perbaikan cetak status
  Serial.print("Fan         : "); Serial.println(finalFanState ? "ON" : "OFF");
  Serial.print("Ventilation : "); Serial.println(finalVentState ? "OPEN" : "CLOSED");

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("SMART ENCLOSURE");
  
  display.setCursor(0, 14);
  display.print("Temp : "); display.print(temperature, 1); display.println(" C");
  
  display.setCursor(0, 26);
  display.print("Hum  : "); display.print(humidity, 1); display.println(" %");
  
  display.setCursor(0, 38);
  display.print("Gas  : "); display.println(gasValue);
  
  display.setCursor(0, 50);
  // Perbaikan layar OLED
  display.print("Fan:"); display.print(finalFanState ? "ON " : "OFF");
  display.print(" Vent:"); display.print(finalVentState ? "OPEN" : "CLOSE");

  display.display();
  delay(2000);
}
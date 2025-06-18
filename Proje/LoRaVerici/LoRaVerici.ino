Kodlar (Verici Lora - Sensör İstasyonu)
#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>
#include "Adafruit_SHT31.h"
#define SS_PIN    5
#define RST_PIN   34
#define DIO0_PIN  35
#define SCK_PIN   18
#define MISO_PIN  19
#define MOSI_PIN  23
// SHT3X - sıcaklık ve nem sensörü
Adafruit_SHT31 sht31 = Adafruit_SHT31();
// HC-SR04
#define TRIG_PIN 25
#define ECHO_PIN 26
// MQ-2
#define MQ2_AO_PIN 33
#define MQ2_DO_PIN 32
// Ses sensörü
#define SOUND_SENSOR_PIN 27
// Kapasitif toprak nem sensörü
#define SOIL_MOISTURE_PIN 14
// Su seviyesi sensörü (analog çıkış)
#define WATER_LEVEL_PIN 12
// PIR hareket sensörü
#define PIR_PIN 13
// LDR - Ortam ışığı sensörü
#define LDR_PIN 4
void setup() {
  Serial.begin(115200);
  // I2C sensör
  Wire.begin(21, 22);
  sht31.begin(0x44);

  // LoRa
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, SS_PIN);
  LoRa.setPins(SS_PIN, RST_PIN, DIO0_PIN);
  if (!LoRa.begin(433E6)) {
    Serial.println("LoRa başlatılamadı!");
    while (true);
  }
  LoRa.setSyncWord(0x0B);
  Serial.println("LoRa Tarla İstasyonu Hazır");
  // Sensör pinleri
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(MQ2_DO_PIN, INPUT);
  pinMode(SOUND_SENSOR_PIN, INPUT);
  pinMode(PIR_PIN, INPUT);
}
// Ses hızı hesaplayan fonksiyon (cm/µs cinsinden)
float calculateSoundSpeed(float temperatureCelsius) {
  return (331.4 + 0.6 * temperatureCelsius) / 10000.0; // cm/µs
}
// Dinamik sıcaklık ile mesafe ölçümü yapan fonksiyon
long readDistanceCM(float temperatureCelsius) {
  float soundSpeed = calculateSoundSpeed(temperatureCelsius);
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH, 30000); // 30 ms timeout
  if (duration == 0) return -1; // mesafe ölçülemedi
  return (duration * soundSpeed) / 2.0;
}
void loop() {
  // Sensörlerden veri okuma
  float temperature = sht31.readTemperature();
  float humidity = sht31.readHumidity();

  int mq2_value = analogRead(MQ2_AO_PIN);
  bool gas_detected = digitalRead(MQ2_DO_PIN);
  int sound_detected = digitalRead(SOUND_SENSOR_PIN);
  int motion_detected = digitalRead(PIR_PIN);
  long distance = readDistanceCM(temperature); // 🔄 sıcaklık parametresi eklendi
  // Toprak nem yüzdesi hesaplama
  int raw_soil = analogRead(SOIL_MOISTURE_PIN);
  int dryValue = 2500;
  int wetValue = 1500;
  int soil_percent = map(raw_soil, dryValue, wetValue, 0, 100);
  soil_percent = constrain(soil_percent, 0, 100);
  // Su seviyesi yüzdesi
  int raw_water_value = analogRead(WATER_LEVEL_PIN);
  int min_water_value = 300;
  int max_water_value = 1023;
  int water_percent = map(raw_water_value, min_water_value, max_water_value, 0, 100);
  water_percent = constrain(water_percent, 0, 100);
  // LDR - Işık yüzdesi
  int raw_ldr = analogRead(LDR_PIN);
  int light_percent = map(raw_ldr, 0, 4095, 0, 100); // 12-bit ADC için
  light_percent = constrain(light_percent, 0, 100);
  // Veriyi JSON formatında hazırla
  String data = "{";
  data += "\"temp\":" + String(temperature, 1) + ",";
  data += "\"hum\":" + String(humidity, 1) + ",";
  data += "\"gas\":" + String(mq2_value) + ",";
  data += "\"gasAlert\":" + String(gas_detected) + ",";
  data += "\"sound\":" + String(sound_detected) + ",";
  data += "\"soil\":" + String(soil_percent) + ",";
  data += "\"water\":" + String(water_percent) + ",";
  data += "\"motion\":" + String(motion_detected) + ",";
  data += "\"distance\":" + String(distance) + ",";
  data += "\"light\":" + String(light_percent);
  data += "}";

  // LoRa ile gönder
  LoRa.beginPacket();
  LoRa.print(data);
  LoRa.endPacket();
  // Seri monitöre yaz
  Serial.println("Gönderiliyor: " + data);
  delay(100); // 1 saniye
}

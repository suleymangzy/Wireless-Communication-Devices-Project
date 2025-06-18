// === Kütüphaneler ===
#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <LiquidCrystal.h>
#include <MFRC522.h>
#include <ArduinoJson.h>

// === Pin Tanımları ===
#define SS_PIN         5
#define RST_PIN        34
#define DIO0_PIN       35
#define SCK_PIN        18
#define MISO_PIN       19
#define MOSI_PIN       23
#define BUZZER_PIN     14
#define BTN_OLED_PAGE  13
#define BTN_LCD_SWITCH 27
#define RST_RFID       26
#define SS_RFID        4

// === Donanım Nesneleri ===
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);
LiquidCrystal lcd(32, 25, 33, 17, 16, 15);
MFRC522 rfid(SS_RFID, RST_RFID);

// === Sistem Değişkenleri ===
bool systemReady = false;
bool warningDetected = false;
bool lastWarningState = false;
bool lcdOnReasonScreen = false;
int currentPage = 1;
String warningMessage = "";

unsigned long previousMillis = 0;
unsigned long lastBtn1Press = 0;
unsigned long lastBtn2Press = 0;
int seconds = 0, minutes = 0, hours = 0;

// === Sensör Verileri ===
String lastTEMP = "--", lastHUM = "--", lastGAS = "--", lastLIGHT = "--", lastDISTANCE = "--";
String lastSOUND = "--", lastWATER = "--", lastMOTION = "--", lastSOIL = "--";

// === Buzzer Fonksiyonu ===
void beepBuzzer(int duration = 200) {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(duration);
  digitalWrite(BUZZER_PIN, LOW);
}

void drawOLEDPage1(const char* timeStr) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);

  u8g2.drawStr(3, 8, "Veri Ekrani - 1");
  u8g2.drawStr(3, 18, ("Saat: " + String(timeStr)).c_str());
  u8g2.drawStr(3, 28, ("Sicaklik: " + lastTEMP + " C").c_str());
  u8g2.drawStr(3, 38, ("Nem: " + lastHUM + " %").c_str());
  u8g2.drawStr(3, 48, ("Toprak: " + lastSOIL + " %").c_str());
  u8g2.drawStr(3, 58, ("Su seviyesi: " + lastWATER + "%").c_str());

  // Sağ kenarı tamamen temizle
  u8g2.setDrawColor(0);
  u8g2.drawBox(116, 0, 12, 64);  // 116. sütundan 128'e kadar sil
  u8g2.setDrawColor(1);

  u8g2.sendBuffer();
}

void drawOLEDPage2(const char* timeStr) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);

  u8g2.drawStr(3, 8, "Veri Ekrani - 2");
  u8g2.drawStr(3, 18, ("Saat: " + String(timeStr)).c_str());
  u8g2.drawStr(3, 28, ("Gaz: " + lastGAS).c_str());
  u8g2.drawStr(3, 38, ("Mesafe: " + lastDISTANCE+"cm").c_str());
  u8g2.drawStr(3, 48, ("Ses: " + lastSOUND).c_str());
  u8g2.drawStr(3, 58, ("Isik Orani: " + lastLIGHT + "%").c_str());

  // Sağ kenarı tamamen temizle
  u8g2.setDrawColor(0);
  u8g2.drawBox(116, 0, 12, 64);  // 116. sütundan 128'e kadar sil
  u8g2.setDrawColor(1);

  u8g2.sendBuffer();
}


void updateLCD() {
  static String lastLine1 = "";
  static String lastLine2 = "";

  String line1, line2;

  if (!warningDetected) {
    line1 = "Her sey stabil";
    line2 = "";
  } else if (lcdOnReasonScreen) {
    line1 = "Sebep Ekrani";
    line2 = "";

    if (warningMessage == "YANGIN TEHLIKESI") {
      line2 = "Gaz: " + lastGAS;
    } else if (warningMessage == "GUVENLIK IHLALI") {
      line2 = "H:" + lastMOTION + " S:" + lastSOUND + " M:" + lastDISTANCE;
    } else if (warningMessage == "TOPRAK NEMI AZ" || warningMessage == "TOPRAK NEMI YUKSEK") {
      line2 = "Nem: " + lastSOIL + "%";
    } else if (warningMessage == "DEPO BOSALIYOR") {
      line2 = "Su: " + lastWATER + "%";
    } else if (warningMessage == "ISIK COK YUKSEK" || warningMessage == "ISIK COK DUSUK") {
      line2 = "Isik: " + lastLIGHT + "%";
    } else if (warningMessage == "SICAKLIK YUKSEK" || warningMessage == "SICAKLIK DUSUK") {
      line2 = "Sicaklik: " + lastTEMP + "C";
    } else if (warningMessage == "NEM COK YUKSEK" || warningMessage == "NEM COK DUSUK") {
      line2 = "NemO: " + lastHUM + "%";
    }

  } else {
    line1 = "Ana Uyari Ekrani";
    line2 = warningMessage;
  }

  if (line1 != lastLine1 || line2 != lastLine2) {
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print(line1);
    lcd.setCursor(0, 1); lcd.print(line2);
    lastLine1 = line1;
    lastLine2 = line2;
  }
}


// === Kurulum ===
void setup() {
  Serial.begin(115200);
  u8g2.begin();
  lcd.begin(16, 2);
  lcd.clear();
  u8g2.clearBuffer();
  u8g2.sendBuffer();

  

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  pinMode(BTN_OLED_PAGE, INPUT_PULLUP);
  pinMode(BTN_LCD_SWITCH, INPUT_PULLUP);

  SPI.begin();
  rfid.PCD_Init();
}

// === Ana Döngü ===
void loop() {
  // Kart Okuma
 // === Giriş ekranı kart okutulana kadar gösterilecek ===
  if (!systemReady) {
    static int introStep = 0;
    static unsigned long lastIntroChange = 0;

    // Kart kontrolü
    if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
      systemReady = true;
      beepBuzzer();
      rfid.PICC_HaltA();
      rfid.PCD_StopCrypto1();
      SPI.end();
      delay(200);

      // LoRa kurulumu
      SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, SS_PIN);
      LoRa.setPins(SS_PIN, RST_PIN, DIO0_PIN);
      int attempts = 0;
      while (!LoRa.begin(433E6) && attempts < 5) {
        lcd.clear(); lcd.print("LoRa baglanmiyor");
        u8g2.clearBuffer(); u8g2.drawStr(0, 20, "LoRa Fail!"); u8g2.sendBuffer();
        delay(2000);
        attempts++;
      }
      LoRa.setSyncWord(0x0B);

      if (attempts >= 5) {
        lcd.clear(); lcd.print("LoRa Basarisiz");
        u8g2.clearBuffer(); u8g2.drawStr(0, 20, "LoRa Basarisiz"); u8g2.sendBuffer();
        while (1); // sistem durur
      }

      lcd.clear(); lcd.print("LoRa Hazir!");
      u8g2.clearBuffer(); u8g2.drawStr(0, 20, "LoRa Hazir!"); u8g2.sendBuffer();
      delay(1000);

      drawOLEDPage1("00:00:00");
      return;
    }

    // 2 saniyede bir ekran değiştir
    if (millis() - lastIntroChange >= 2000 && introStep < 5) {
      lastIntroChange = millis();
      lcd.clear();
      u8g2.clearBuffer();
      u8g2.setFont(u8g2_font_ncenB08_tr);

      switch (introStep) {
        case 0:
          lcd.setCursor(0, 0);
          lcd.print("Akilli Tarim ve");
          lcd.setCursor(0, 1);
          lcd.print("Guvenlik Izleme");
          u8g2.setFont(u8g2_font_ncenB08_tr);
          u8g2.drawStr(0, 20, "Akilli Tarim ve");
          u8g2.drawStr(0, 40, "Guvenlik Izleme");
          break;
        case 1:
          lcd.setCursor(0, 0);
          lcd.print("Merhaba");
          u8g2.setFont(u8g2_font_ncenB08_tr);
          u8g2.drawStr(0, 20, "Merhaba");
          break;
        case 2:
          lcd.setCursor(0, 0);
          lcd.print("Uyari ve Kontrol");
          lcd.setCursor(0, 1);
          lcd.print("Ekrani");
          u8g2.setFont(u8g2_font_ncenB08_tr);
          u8g2.drawStr(0, 20, "Veri Ekrani");
          break;
        case 3:
          lcd.setCursor(0, 0);
          lcd.print("Suleyman Guzey");
          lcd.setCursor(0, 1);
          lcd.print("Elif Saglam");
          u8g2.setFont(u8g2_font_ncenB08_tr);
          u8g2.drawStr(0, 20, "2212101038");
          u8g2.drawStr(0, 40, "2212101024");
          break;
        case 4:
          lcd.setCursor(0, 0);
          lcd.print("Karti Okutun...");
          u8g2.setFont(u8g2_font_ncenB08_tr);
          u8g2.drawStr(0, 20, "Karti Okutun...");
          break;
      }

      lcd.display(); // Bazı LCD modelleri için gerekebilir
      u8g2.sendBuffer();

      introStep++;
    }

    return;
  }

  // Zaman Güncelleme
  if (millis() - previousMillis >= 1000) {
    previousMillis = millis();
    if (++seconds >= 60) {
      seconds = 0;
      if (++minutes >= 60) {
        minutes = 0;
        if (++hours >= 24) hours = 0;
      }
    }
  }

  char timeStr[9];
  sprintf(timeStr, "%02d:%02d:%02d", hours, minutes, seconds);

  // Buton Okuma
  bool btn1Pressed = digitalRead(BTN_OLED_PAGE) == LOW;
  bool btn2Pressed = digitalRead(BTN_LCD_SWITCH) == LOW;

  if (btn1Pressed && millis() - lastBtn1Press > 300) {
    lastBtn1Press = millis();
    currentPage = (currentPage == 1) ? 2 : 1;
    (currentPage == 1) ? drawOLEDPage1(timeStr) : drawOLEDPage2(timeStr);
  }

  if (btn2Pressed && millis() - lastBtn2Press > 300) {
    lastBtn2Press = millis();
    if (warningDetected) {
      lcdOnReasonScreen = !lcdOnReasonScreen;
      updateLCD();
    }
  }   

  // LoRa Veri Alımı
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String receivedMessage = "";
    while (LoRa.available()) {
      receivedMessage += (char)LoRa.read();
    }

    Serial.println("Gelen veri: " + receivedMessage);

    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, receivedMessage);
    if (!error) {
      String newTEMP = String(doc["temp"].as<float>(), 1);
      String newHUM = String(doc["hum"].as<float>(), 1);
      String newGAS = String(doc["gas"].as<String>());
      String newSOUND = String(doc["sound"].as<int>());
      String newWATER = String(doc["water"].as<int>());
      String newMOTION = String(doc["motion"].as<int>());
      String newSOIL = String(doc["soil"].as<int>());
      String newLIGHT = String(doc["light"].as<int>());
      String newDISTANCE = String(doc["distance"].as<int>());

      // Sensör verilerini al
      int temp = doc["temp"].as<float>();
      int hum = doc["hum"].as<float>();
      int gas = doc["gas"].as<int>();
      int sound = doc["sound"].as<int>();
      int water = doc["water"].as<int>();
      int motion = doc["motion"].as<int>();
      int soil = doc["soil"].as<int>();
      int light = doc["light"].as<int>();
      int distance = doc["distance"].as<int>();

  // Uyarı kontrolü
  warningDetected = false;
  warningMessage = "";

  if (gas > 2500) {
    warningDetected = true;
    warningMessage = "YANGIN TEHLIKESI";
    beepBuzzer(700);
  } else if (motion == 1 && sound > 1000 && distance <= 30) {
    warningDetected = true;
    warningMessage = "GUVENLIK IHLALI";
    beepBuzzer(700);
  } else if (soil < 10) {
    warningDetected = true;
    warningMessage = "TOPRAK NEMI AZ";
    beepBuzzer(700);
  } else if (soil > 75) {
    warningDetected = true;
    warningMessage = "TOPRAK NEMI YUKSEK";
    beepBuzzer(700);
  } else if (water <= 20) {
    warningDetected = true;
    warningMessage = "DEPO BOSALIYOR";
    beepBuzzer(700);
  } else if (light > 90) {
    warningDetected = true;
    warningMessage = "ISIK COK YUKSEK";
    beepBuzzer(700);
  } else if (light < 30) {
    warningDetected = true;
    warningMessage = "ISIK COK DUSUK";
    beepBuzzer(700);
  } else if (temp >= 30) {
    warningDetected = true;
    warningMessage = "SICAKLIK YUKSEK";
    beepBuzzer(700);
  } else if (temp <= 15) {
    warningDetected = true;
    warningMessage = "SICAKLIK DUSUK";
    beepBuzzer(700);
  } else if (hum >= 75) {
    warningDetected = true;
    warningMessage = "NEM COK YUKSEK";
    beepBuzzer(700);
  } else if (hum <= 10) {
    warningDetected = true;
    warningMessage = "NEM COK DUSUK";
    beepBuzzer(700);
  }

  lastWarningState = warningDetected;

  bool shouldUpdateOLED = false;
  if (currentPage == 1 && (newTEMP != lastTEMP || newHUM != lastHUM || newWATER != lastWATER || newSOIL != lastSOIL)) {
      shouldUpdateOLED = true;
    }
  if (currentPage == 2 && (newGAS != lastGAS || newDISTANCE != lastDISTANCE || newSOUND != lastSOUND || newLIGHT != lastLIGHT)) {
       shouldUpdateOLED = true;
    }

  lastTEMP = newTEMP;
  lastHUM = newHUM;
  lastGAS = newGAS;
  lastSOUND = newSOUND;
  lastWATER = newWATER;
  lastMOTION = newMOTION;
  lastSOIL = newSOIL;
  lastLIGHT = newLIGHT;
  lastDISTANCE = newDISTANCE;

  if (shouldUpdateOLED) {
    (currentPage == 1) ? drawOLEDPage1(timeStr) : drawOLEDPage2(timeStr);
  }
  updateLCD();
  } else {
    Serial.println("JSON ayrıştırma hatası!");
  }
}
}


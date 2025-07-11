#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SPI.h>
#include <MFRC522.h>

#define RST_PIN 9
#define SS_PIN 10

byte readCard[4];

// OLED settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1  // Usually -1 if not using a reset pin
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

MFRC522 mfrc522(SS_PIN, RST_PIN);

void setup() {
  Serial.begin(9600);

  // Initialize OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    while (1);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Put your card");
  display.display();

  SPI.begin();
  mfrc522.PCD_Init();
  delay(4);
  mfrc522.PCD_DumpVersionToSerial();
}

void loop() {
  if (!mfrc522.PICC_IsNewCardPresent()) {
    return;
  }
  if (!mfrc522.PICC_ReadCardSerial()) {
    return;
  }

  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Scanned UID:");

  int cursorX = 0;
  int cursorY = 16;

  Serial.print(F("Scanned PICC's UID: "));
  for (uint8_t i = 0; i < 4; i++) {
    readCard[i] = mfrc522.uid.uidByte[i];
    
    // Print UID byte to Serial in uppercase HEX with leading zero
    if (readCard[i] < 0x10) Serial.print("0");
    Serial.print(readCard[i], HEX);
    Serial.print(" ");

    // Print UID byte to OLED
    if (readCard[i] < 0x10) display.print("0");
    display.print(readCard[i], HEX);
    display.print(" ");

    cursorX += 24;  // Move cursor right for next byte (approx 6x4 chars)
    if (cursorX >= SCREEN_WIDTH - 24) {  // Prevent going off screen
      cursorX = 0;
      cursorY += 10;
      display.setCursor(cursorX, cursorY);
    }
  }
  Serial.println();

  display.display();

  mfrc522.PICC_HaltA();  // Halt PICC
  delay(1000);           // Add a short delay to avoid flooding
}

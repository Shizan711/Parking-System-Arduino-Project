#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Servo.h>

// OLED configuration
#define SCREEN_WIDTH        128
#define SCREEN_HEIGHT        64

// Pins
#define SS_PIN               10
#define RST_PIN               9
#define SERVO_PIN             4

#define PROXIMITY_ENTRY_PIN   2   // Interrupt pin (must be 2 or 3 on Uno)
#define IR_EXIT_PIN           8   // Direct IR sensor output for exit (active LOW)
#define BUZZER_PIN            3

#define GATE_CLOSED_ANGLE     0
#define GATE_OPEN_ANGLE      90

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
MFRC522 rfid(SS_PIN, RST_PIN);
Servo myservo;

const int MaxSlots = 5;
int slotsAvailable = MaxSlots;

volatile bool entryDetected = false;
bool carEntering = false;
bool carExiting = false;

const byte authorizedUIDs[5][4] = {
  {0xAD, 0xC7, 0xE1, 0x9E},
  {0x03, 0x4E, 0x2B, 0x55},
  {0x36, 0xEE, 0x93, 0xFE},
  {0xF0, 0x32, 0xAB, 0x12},
  {0xC1, 0xD2, 0x56, 0x78}
};

const byte authorizedUIDLength = 4;
const byte totalAuthorized = 5;
const unsigned long irWaitTimeout = 5000;

void setup() {
  Serial.begin(9600);

  pinMode(PROXIMITY_ENTRY_PIN, INPUT);
  pinMode(IR_EXIT_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  noTone(BUZZER_PIN);

  myservo.attach(SERVO_PIN);
  myservo.write(GATE_CLOSED_ANGLE);

  // Initialize OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 failed"));
    while (1);
  }

  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("AIUB CAR");
  display.setCursor(0, 20);
  display.println("PARKING");
  display.setCursor(0, 40);
  display.println("SYSTEM");
  display.display();
  delay(3000);
  display.clearDisplay();

  SPI.begin();
  rfid.PCD_Init();

  attachInterrupt(digitalPinToInterrupt(PROXIMITY_ENTRY_PIN), entryISR, RISING);
  displaySlots();
}

void loop() {
  // Entry via interrupt flag
  if (entryDetected && !carEntering && !carExiting) {
    entryDetected = false;
    carEntering = true;
    Serial.println("Car detected at entry!");
    handleCarEntry();
    carEntering = false;
  }

  // Exit via polling
  if (digitalRead(IR_EXIT_PIN) == LOW && !carExiting && !carEntering) {
    carExiting = true;
    Serial.println("Car detected at exit!");
    handleCarExit();
    carExiting = false;
    delay(1000);
  }

  delay(5); // Small delay for stability
}

// Interrupt Service Routine for entry detection
void entryISR() {
  entryDetected = true;
}

void handleCarEntry() {
  displayMessage("Car Detected!");
  delay(500);

  if (slotsAvailable <= 0) {
    displayMessage("Parking Full!");
    beepBuzzer(false);
    delay(1500);
    displaySlots();
    return;
  }

  if (scanRFID() == 1) {
    beepBuzzer(true);
    displayMessage("Gate Opened!");
    openGateSmooth();
    slotsAvailable--;
    delay(2000);
    waitForCarPass(IR_EXIT_PIN);
    closeGateSmooth();
    displayMessage("Gate Closed!");
    delay(1500);
  }

  displaySlots();
}

void handleCarExit() {
  if (slotsAvailable >= MaxSlots) {
    displayMessage("No Cars Left");
    beepBuzzer(false);
    delay(1500);
    displaySlots();
    return;
  }

  displayMessage("Car Exiting!");
  delay(1000);

  displayMessage("Gate Opened!");
  openGateSmooth();
  delay(2000);

  slotsAvailable++;
  waitForCarPass(PROXIMITY_ENTRY_PIN);
  closeGateSmooth();
  displayMessage("Gate Closed!");
  delay(1500);

  displaySlots();
}

void displayHeader() {
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.setTextColor(SSD1306_WHITE);
  display.println(F(" AIUB PARKING SYSTEM"));
  display.drawFastHLine(0, 10, SCREEN_WIDTH, SSD1306_WHITE);
}

void displaySlots() {
  display.clearDisplay();
  displayHeader();
  display.setTextSize(2);
  display.setCursor(0, 20);
  display.println("Slots Left");

  display.setTextSize(3);
  int x = (SCREEN_WIDTH - 36) / 2;
  display.setCursor(x, 45);

  if (slotsAvailable == 0) {
    display.println("FULL");
  } else {
    display.print(slotsAvailable);
    display.print("/");
    display.print(MaxSlots);
  }

  display.display();
}

void displayMessage(const char* msg) {
  display.clearDisplay();
  displayHeader();
  display.setTextSize(2);
  display.setCursor(0, 25);
  display.println(msg);
  display.display();
}

void openGateSmooth() {
  myservo.write(GATE_OPEN_ANGLE);
}

void closeGateSmooth() {
  myservo.write(GATE_CLOSED_ANGLE);
}

void waitForCarPass(int pin) {
  unsigned long start = millis();

  if (pin == IR_EXIT_PIN) {
    while (digitalRead(pin) == HIGH && millis() - start < irWaitTimeout) delay(10);
    start = millis();
    while (digitalRead(pin) == LOW && millis() - start < irWaitTimeout) delay(10);
  } else {
    while (digitalRead(pin) == LOW && millis() - start < irWaitTimeout) delay(10);
    start = millis();
    while (digitalRead(pin) == HIGH && millis() - start < irWaitTimeout) delay(10);
  }
}

int scanRFID() {
  displayMessage("Scan Card");
  unsigned long start = millis();

  while (millis() - start < 10000) {
    if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
      Serial.print("UID: ");
      for (byte i = 0; i < rfid.uid.size; i++) {
        Serial.print(rfid.uid.uidByte[i], HEX);
        Serial.print(" ");
      }
      Serial.println();

      display.clearDisplay();
      display.setTextSize(1);
      display.setCursor(0, 0);
      display.println("Card Scanned:");
      display.setCursor(0, 20);
      for (byte i = 0; i < rfid.uid.size; i++) {
        if (rfid.uid.uidByte[i] < 0x10) display.print("0");
        display.print(rfid.uid.uidByte[i], HEX);
        display.print(" ");
      }
      display.display();
      delay(1000);

      if (rfid.uid.size == authorizedUIDLength) {
        for (int j = 0; j < totalAuthorized; j++) {
          bool match = true;
          for (int i = 0; i < authorizedUIDLength; i++) {
            if (rfid.uid.uidByte[i] != authorizedUIDs[j][i]) {
              match = false;
              break;
            }
          }
          if (match) {
            displayMessage("Access Granted");
            delay(1500);
            rfid.PICC_HaltA();
            return 1;
          }
        }
      }

      displayMessage("Access Denied");
      beepBuzzer(false);
      delay(1500);
      rfid.PICC_HaltA();
      return 0;
    }
    delay(100);
  }

  displayMessage("Scan Timeout");
  delay(1500);
  return 0;
}

void beepBuzzer(bool granted) {
  if (granted) {
    tone(BUZZER_PIN, 1200, 200);
    delay(300);
  } else {
    tone(BUZZER_PIN, 500, 2000);
    delay(2100);
  }
  noTone(BUZZER_PIN);
}

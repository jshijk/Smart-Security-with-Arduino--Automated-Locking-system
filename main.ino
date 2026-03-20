#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include <MFRC522.h>

#define SS_PIN 10
#define RST_PIN 9

// ===== YOUR CARD UID =====
String authorizedUID = "37 21 35 25";

// ===== HARDWARE PINS =====
const int RELAY_PIN = 7;

// ===== SWIPE SETTINGS =====
const unsigned long SWIPE_TIMEOUT = 2000;      // Max time between swipes
const int PATTERN_LENGTH = 3;                   // Left-Right-Left

// ===== SYSTEM VARIABLES =====
bool doorUnlocked = false;
int swipeCount = 0;
unsigned long lastSwipeTime = 0;
bool cardWasPresent = false;

// LCD and RFID objects
LiquidCrystal_I2C lcd(0x27, 16, 2);
MFRC522 rfid(SS_PIN, RST_PIN);

void setup() {
  Serial.begin(9600);
  
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  
  lcd.init();
  lcd.backlight();
  SPI.begin();
  rfid.PCD_Init();
  
  // Welcome
  lcd.setCursor(0, 0);
  lcd.print("Swipe Pattern");
  lcd.setCursor(0, 1);
  lcd.print("Left-Right-Left");
  delay(2500);
  lcd.clear();
  
  Serial.println("Ready - Swipe Left-Right-Left to toggle lock");
}

void loop() {
  updateDisplay();
  
  // Check for card
  bool cardPresent = rfid.PICC_IsNewCardPresent();
  
  if (cardPresent) {
    if (!rfid.PICC_ReadCardSerial()) {
      return;
    }
    
    String cardID = getCardID();
    
    // Check authorization
    if (cardID != authorizedUID) {
      handleWrongCard();
      rfid.PICC_HaltA();
      return;
    }
    
    // Valid card - count this swipe
    handleSwipe();
    rfid.PICC_HaltA();
  } else {
    cardWasPresent = false;
  }
  
  // Check timeout
  if (swipeCount > 0 && millis() - lastSwipeTime > SWIPE_TIMEOUT) {
    swipeTimeout();
  }
}

void handleSwipe() {
  unsigned long now = millis();
  
  // Reset if too much time passed
  if (swipeCount > 0 && now - lastSwipeTime > SWIPE_TIMEOUT) {
    swipeCount = 0;
  }
  
  swipeCount++;
  lastSwipeTime = now;
  
  // Show progress
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Swipe ");
  lcd.print(swipeCount);
  lcd.print("/");
  lcd.print(PATTERN_LENGTH);
  
  // Check which swipe this should be
  bool correctSwipe = false;
  
  if (swipeCount == 1) {
    // Should be LEFT
    lcd.setCursor(0, 1);
    lcd.print("Left OK");
    correctSwipe = true;
  } else if (swipeCount == 2) {
    // Should be RIGHT
    lcd.setCursor(0, 1);
    lcd.print("Right OK");
    correctSwipe = true;
  } else if (swipeCount == 3) {
    // Should be LEFT
    lcd.setCursor(0, 1);
    lcd.print("Left OK!");
    delay(300);
    
    // Pattern complete!
    if (!doorUnlocked) {
      unlockDoor();
    } else {
      lockDoor();
    }
    swipeCount = 0;
    return;
  }
  
  if (correctSwipe) {
    delay(300);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Need ");
    lcd.print(PATTERN_LENGTH - swipeCount);
    lcd.print(" more...");
    lcd.setCursor(0, 1);
    if (swipeCount == 1) {
      lcd.print("Next: Right");
    } else if (swipeCount == 2) {
      lcd.print("Next: Left");
    }
  }
  
  Serial.print("Swipe ");
  Serial.print(swipeCount);
  Serial.println(" detected");
}

void swipeTimeout() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Too slow!");
  lcd.setCursor(0, 1);
  lcd.print("Start over");
  delay(1500);
  swipeCount = 0;
  lcd.clear();
}

void updateDisplay() {
  if (swipeCount > 0) return; // Don't overwrite swipe feedback
  
  if (doorUnlocked) {
    lcd.setCursor(0, 0);
    lcd.print("UNLOCKED        ");
    lcd.setCursor(0, 1);
    lcd.print("L-R-L to LOCK   ");
  } else {
    lcd.setCursor(0, 0);
    lcd.print("LOCKED          ");
    lcd.setCursor(0, 1);
    lcd.print("L-R-L to OPEN   ");
  }
}

void unlockDoor() {
  digitalWrite(RELAY_PIN, HIGH);
  doorUnlocked = true;
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ACCESS GRANTED! ");
  lcd.setCursor(0, 1);
  lcd.print("Door UNLOCKED   ");
  delay(2000);
  lcd.clear();
  
  Serial.println("*** DOOR UNLOCKED ***");
}

void lockDoor() {
  digitalWrite(RELAY_PIN, LOW);
  doorUnlocked = false;
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("LOCKED!         ");
  lcd.setCursor(0, 1);
  lcd.print("Goodbye         ");
  delay(1500);
  lcd.clear();
  
  Serial.println("*** DOOR LOCKED ***");
}

void handleWrongCard() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WRONG CARD      ");
  lcd.setCursor(0, 1);
  lcd.print("Access Denied   ");
  delay(2000);
  lcd.clear();
  
  // Reset pattern on wrong card
  swipeCount = 0;
}

String getCardID() {
  String ID = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) ID.concat("0");
    ID.concat(String(rfid.uid.uidByte[i], HEX));
    if (i < rfid.uid.size - 1) ID.concat(" ");
  }
  ID.toUpperCase();
  return ID;
}
